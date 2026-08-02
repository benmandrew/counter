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
    }
    // print_hoa leaves off the trailing newline the tool emits after --END--.
    SpotTranslation translation;
    translation.m_hoa = out.str() + "\n";
    return translation;
}

// Where an abandoned worker leaves its answer for a caller that may no longer
// be there to read it. Held by shared_ptr so the worker keeps it alive on its
// own: after the deadline the caller is gone and this is all that remains.
struct TranslationSlot {
    std::mutex m_mutex;
    std::condition_variable m_ready;
    bool m_done = false;
    bool m_abandoned = false;
    SpotTranslation m_result;
};

// Runs the translation on its own thread, taking over the already-held libspot
// lock, and waits @p deadline for it. The lock is released by whoever finishes
// the work, which past the deadline is nobody this call can still see.
SpotTranslation translate_with_deadline(const std::string& formula,
                                        std::chrono::milliseconds deadline) {
    auto slot = std::make_shared<TranslationSlot>();
    std::thread worker([slot, formula] {
        // The caller's scope around this call measures the caller's thread, so
        // once the work moves here its CPU stops being attributed to it -- the
        // translate site would report wall with almost no CPU, which is the
        // profiler's signal for "blocked", and would be exactly wrong. This
        // site is where the CPU actually goes; the caller's remains the wall
        // time a scoring thread lost, deadline wait included.
        COUNTER_PROFILE_SCOPE("spot/libspot-translate-worker");
        SpotTranslation result;
        {
            // adopt_lock, not lock: the mutex was taken by the caller and
            // handed over unlocked-by-nobody. Holding it in a guard means it is
            // released even if the translation throws -- a std::bad_alloc on a
            // formula large enough to exhaust memory is exactly the case where
            // leaving libspot permanently locked would be worst.
            const std::unique_lock<std::timed_mutex> guard(spot_mutex(),
                                                           std::adopt_lock);
            try {
                result = translate_locked(formula);
            } catch (...) {
                result = SpotTranslation{};
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
            g_abandoned_workers.fetch_sub(1, std::memory_order_relaxed);
        }
    });
    worker.detach();

    std::unique_lock<std::mutex> wait(slot->m_mutex);
    if (slot->m_ready.wait_for(wait, deadline,
                               [&slot] { return slot->m_done; })) {
        return std::move(slot->m_result);
    }
    // Marked under the same mutex the worker will take, so there is no window
    // where both sides believe the other owns the result.
    slot->m_abandoned = true;
    g_abandoned_workers.fetch_add(1, std::memory_order_relaxed);
    SpotTranslation abandoned;
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
                                     std::chrono::milliseconds budget) {
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotSimplification result;
        result.m_lock_busy = true;
        return result;
    }
    SpotSimplification result;
    result.m_formula = simplify_locked(formula);
    return result;
}

SpotTranslation spot_translate_for_counting(
    const std::string& formula, std::chrono::milliseconds budget,
    std::chrono::milliseconds deadline) {
    COUNTER_PROFILE_SCOPE("spot/libspot-translate");
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotTranslation busy;
        busy.m_lock_busy = true;
        return busy;
    }
    if (deadline == std::chrono::milliseconds::zero()) {
        // No deadline asked for, so nothing to enforce and no reason to pay a
        // thread for it: run here, exactly as an unbounded exec would have.
        return translate_locked(formula);
    }
    // Hands the lock to the worker without unlocking it. From here the mutex is
    // held by a thread that is not this one, and only that thread may release
    // it -- which is what keeps an abandoned translation from being joined by a
    // second one on the same libspot state.
    lock.release();
    return translate_with_deadline(formula, deadline);
}

std::size_t spot_abandoned_workers() {
    return g_abandoned_workers.load(std::memory_order_relaxed);
}
