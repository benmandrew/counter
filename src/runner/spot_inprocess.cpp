#include "runner/spot_inprocess.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/tl/simplify.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/translate.hh>

#include "profile.hpp"

namespace {

// Leaked on purpose, and never destroyed. An abandoned translation worker (see
// spot_translate_for_counting) can still be inside libspot when static
// destruction begins, and it unlocks this mutex whenever it finally finishes.
// Destroying a mutex that a live thread still holds is undefined behaviour, and
// it would happen at the one moment the process was otherwise exiting cleanly.
std::timed_mutex& spot_mutex() {
    static auto& mutex = *new std::timed_mutex();
    return mutex;
}

// Read by anything about to tear down state an abandoned worker may still be
// using -- main's leave(), and the tests that wait for one to finish. That is
// why the decrement is a release and the load an acquire rather than both being
// relaxed: relaxed makes the count temporally right and formally useless, since
// a reader seeing zero would still have no ordering against the libspot the
// worker had been walking. ThreadSanitizer says so directly, reporting SPOT's
// intern table being destroyed at exit against the worker's last read of it.
std::atomic<std::size_t> g_abandoned_workers{0};

// Runs the simplification. The caller must already hold spot_mutex().
std::optional<std::string> simplify_locked(const std::string& formula) {
    // Constructed here rather than at namespace scope: building a
    // tl_simplifier touches the same SPOT globals its use does, and doing that
    // unserialised crashes. The function-local static's own guard orders
    // construction, but does not order it against another thread already
    // simplifying, so it has to sit inside the lock.
    // Level 3 matches `ltlfilt --simplify`; the default options do not.
    // Leaked for the same reason as the mutex above: this holds formula nodes
    // an abandoned worker may still be walking at exit.
    static auto& simplifier =
        *new spot::tl_simplifier(spot::tl_simplifier_options(3));
    const spot::parsed_formula parsed = spot::parse_infix_psl(formula);
    if (!parsed.errors.empty() || !parsed.f) {
        return std::nullopt;
    }
    return spot::str_psl(simplifier.simplify(parsed.f));
}

// Runs the translation. The caller must already hold spot_mutex().
SpotTranslation translate_locked(const std::string& formula) {
    const spot::parsed_formula parsed = spot::parse_infix_psl(formula);
    if (!parsed.errors.empty() || !parsed.f) {
        return {};
    }
    // -D and -S: prefer a deterministic automaton with state-based acceptance.
    // A fresh bdd_dict per call, so atomic propositions are numbered from this
    // formula alone and match what the tool prints.
    spot::translator translator{spot::make_bdd_dict()};
    translator.set_pref(spot::postprocessor::Deterministic |
                        spot::postprocessor::SBAcc);
    std::ostringstream out;
    try {
        spot::print_hoa(out, translator.run(parsed.f), "");
    } catch (const std::runtime_error& error) {
        // Not an error in the formula: the automaton is correct and universal,
        // and only printing refuses it. Reported rather than swallowed, so the
        // caller substitutes the universal automaton instead of counting a
        // genuinely-true formula as a scoring failure.
        if (std::string(error.what())
                .find("automaton is complete but prop_complete()") !=
            std::string::npos) {
            SpotTranslation translation;
            translation.m_tautology_print_bug = true;
            return translation;
        }
        // Any other libspot failure is reported as "no automaton" rather than
        // rethrown, which sends the caller to the exec. That is the reference
        // behaviour this path replaced, so falling back to it keeps in-process
        // translation from ever being the worse of the two: whatever ltl2tgba
        // makes of the formula, including failing, is what happens.
        return {};
    } catch (...) {
        // std::bad_alloc above all, which is not hypothetical: this formula
        // family reaches tens of megabytes and the exec path never spent them
        // out of *this* process's heap. Handled identically to a libspot error
        // -- fall back to the tool, whose own address space may well have the
        // room this one has run out of.
        //
        // Catching it here rather than only in the worker below is what keeps
        // the two paths the same. Letting it propagate meant a formula that ran
        // out of memory dropped the individual when no deadline was set and
        // fell back to the exec when one was, which is a behavioural difference
        // no caller asked for and nothing would have reported.
        return {};
    }
    // print_hoa leaves off the trailing newline the tool emits after --END--.
    SpotTranslation translation;
    translation.m_hoa = out.str() + "\n";
    return translation;
}

// Where an abandoned worker leaves its answer for a caller that may no longer
// be there to read it. Held by shared_ptr so the worker keeps it alive on its
// own: after the deadline the caller is gone and this is all that remains.
template <typename Result>
struct DeadlineSlot {
    std::mutex m_mutex;
    std::condition_variable m_ready;
    bool m_done = false;
    bool m_abandoned = false;
    Result m_result;
};

// Runs @p work on its own thread, holding the libspot lock, and waits
// @p deadline for it. @p Result is whichever outcome struct the call returns;
// it needs an m_lock_busy and an m_timed_out.
//
// One copy for both entry points on purpose. What is delicate here is the
// handover, not the work, and a second copy would be a second chance to get it
// wrong in a way only ThreadSanitizer notices.
//
// The worker takes the libspot lock itself rather than being handed one the
// caller already held. Handing it over is the obvious design and it is wrong:
// std::timed_mutex may only be unlocked by the thread that locked it, and the
// worker is by definition the thread that has to do the unlocking, since past
// the deadline the caller is gone. ThreadSanitizer reports the handover as
// "unlock of an unlocked mutex (or by a wrong thread)", which is exactly what
// it is. Locking here costs a thread that turns out not to be needed when the
// lock is busy, which against an 8ms budget is not worth avoiding.
//
// The caller therefore waits @p budget as well as @p deadline: up to the first
// is spent getting the lock, and only then does the work start. By the time the
// two have elapsed the worker has either taken the lock or given up, so a
// timeout here always means a call that ran and did not finish.
//
// @p work is copied into the worker, and must own everything it touches: past
// the deadline the caller has returned and its locals are gone.
template <typename Result, typename Work>
Result run_with_deadline(std::chrono::milliseconds budget,
                         std::chrono::milliseconds deadline, const Work& work) {
    auto slot = std::make_shared<DeadlineSlot<Result>>();
    std::thread worker([slot, work, budget] {
        Result result;
        {
            // Held in a guard so the lock is released even if the work throws.
            // A std::bad_alloc on a formula large enough to exhaust memory is
            // exactly the case where leaving libspot permanently locked would
            // be worst.
            std::unique_lock<std::timed_mutex> guard(spot_mutex(),
                                                     std::defer_lock);
            if (!guard.try_lock_for(budget)) {
                result.m_lock_busy = true;
            } else {
                try {
                    result = work();
                } catch (...) {
                    result = Result{};
                }
            }
        }
        bool was_abandoned = false;
        {
            const std::scoped_lock lock(slot->m_mutex);
            slot->m_result = std::move(result);
            slot->m_done = true;
            was_abandoned = slot->m_abandoned;
        }
        slot->m_ready.notify_one();
        if (was_abandoned) {
            g_abandoned_workers.fetch_sub(1, std::memory_order_release);
        }
    });
    worker.detach();

    std::unique_lock<std::mutex> wait(slot->m_mutex);
    if (slot->m_ready.wait_for(wait, budget + deadline,
                               [&slot] { return slot->m_done; })) {
        return std::move(slot->m_result);
    }
    // Marked under the same mutex the worker will take, so there is no window
    // where both sides believe the other owns the result.
    slot->m_abandoned = true;
    g_abandoned_workers.fetch_add(1, std::memory_order_relaxed);
    Result abandoned;
    abandoned.m_timed_out = true;
    return abandoned;
}

}  // namespace

std::optional<std::string> spot_simplify(const std::string& formula) {
    // Outside the lock deliberately, so the recorded wall time includes waiting
    // for it. Wall far above CPU here is the signal that serialising has begun
    // to cost something.
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    const std::scoped_lock lock(spot_mutex());
    return simplify_locked(formula);
}

SpotSimplification spot_try_simplify(const std::string& formula,
                                     std::chrono::milliseconds budget,
                                     std::chrono::milliseconds deadline) {
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    if (deadline != std::chrono::milliseconds::zero()) {
        return run_with_deadline<SpotSimplification>(
            budget, deadline, [formula] {
                // The caller's scope above measures the caller's thread, so
                // once the work moves here its CPU stops being attributed to
                // it -- the simplify site would report wall with almost no
                // CPU, which is the profiler's signal for "blocked", and would
                // be exactly wrong. This site is where the CPU actually goes;
                // the caller's remains the wall time a scoring thread lost,
                // deadline wait included.
                COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify-worker");
                SpotSimplification result;
                result.m_formula = simplify_locked(formula);
                return result;
            });
    }
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotSimplification result;
        result.m_lock_busy = true;
        return result;
    }
    // No deadline asked for, so nothing to enforce and no reason to pay a
    // thread for it: run here, exactly as an unbounded exec would have.
    SpotSimplification result;
    result.m_formula = simplify_locked(formula);
    return result;
}

SpotTranslation spot_translate_for_counting(
    const std::string& formula, std::chrono::milliseconds budget,
    std::chrono::milliseconds deadline) {
    COUNTER_PROFILE_SCOPE("spot/libspot-translate");
    if (deadline != std::chrono::milliseconds::zero()) {
        // The worker takes the lock itself, so this must not take it first --
        // see run_with_deadline for why handing one over is not an option.
        return run_with_deadline<SpotTranslation>(budget, deadline, [formula] {
            // Where the CPU actually goes; see the note in spot_try_simplify.
            COUNTER_PROFILE_SCOPE("spot/libspot-translate-worker");
            return translate_locked(formula);
        });
    }
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotTranslation busy;
        busy.m_lock_busy = true;
        return busy;
    }
    // No deadline asked for, so nothing to enforce and no reason to pay a
    // thread for it: run here, exactly as an unbounded exec would have.
    return translate_locked(formula);
}

std::size_t spot_abandoned_workers() {
    return g_abandoned_workers.load(std::memory_order_acquire);
}
