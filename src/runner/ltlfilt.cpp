#include "runner/ltlfilt.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "profile.hpp"
#include "runner/simplify_batcher.hpp"
#include "runner/spot.hpp"
#include "runner/spot_inprocess.hpp"
#include "runner/subprocess.hpp"

namespace {

std::atomic<SimplifyEngine> g_simplify_engine{SimplifyEngine::Libspot};

// Per-formula wall-clock budget for a simplification, in milliseconds; 0
// disables it. Set once at startup from Config::simplify_timeout, read by every
// scoring worker, hence atomic. Applies to both engines, because the hazard is
// the same on both: `--simplify` has no internal bound and blows up
// super-exponentially on deeply nested-X conjunctions.
std::atomic<std::int64_t> g_simplify_timeout_ms{0};

// How long a caller will wait for the libspot lock before spawning ltlfilt
// instead. Set to roughly what a spawn costs (measured at 8-9 ms, dominated by
// the child demand-paging its own executable), because that is exactly the
// point where waiting stops being the cheaper of the two.
constexpr std::chrono::milliseconds k_libspot_lock_budget{8};

// How long to hold the lock open for when there is no ltlfilt to spawn instead.
// Not a deadline -- the wait repeats for as long as the lock is legitimately
// contended -- only how often the reason for waiting is re-examined. A second
// is short enough that a run cannot sit on a dead lock for meaningfully longer
// than that, and long enough that re-checking costs nothing.
constexpr std::chrono::milliseconds k_no_fallback_wait_slice{1000};

// Counted because nothing else tells the two apart. A timed-out simplification
// caches the formula unchanged and the run carries on, so a simplify_timeout_ms
// set too tight has no symptom at all beyond a search quietly working on
// unsimplified formulae. A free function rather than a branch at the call site,
// which is already at the cognitive-complexity limit.
void count_simplify_timeout(const SpotSimplification& result) {
    if (result.m_timed_out) {
        profile::add_count("libspot/simplify-timed-out");
    }
}

}  // namespace

std::string ltlfilt_path() { return spot_bin_dir() + "/ltlfilt"; }

void set_simplify_engine(SimplifyEngine engine) {
    g_simplify_engine.store(engine);
}

void set_simplify_timeout(std::chrono::milliseconds timeout) {
    g_simplify_timeout_ms.store(timeout.count());
}

std::string simplify_ltl(const std::string& formula) {
    COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl");
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl:cache-lookup");
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            LtlfiltStats::n_cache_hits++;
            return found->second;
        }
        LtlfiltStats::n_cache_misses++;
    }
    const auto start = std::chrono::steady_clock::now();
    const auto timeout =
        std::chrono::milliseconds(g_simplify_timeout_ms.load());
    // The in-process engine needs no binary on disk and spawns nothing, so it
    // is tried before the ltlfilt path is even looked for. A formula it cannot
    // parse is cached and returned unchanged rather than sent to the exec:
    // that is already what the exec did with a formula ltlfilt could not parse,
    // so spawning to reach the same answer would only cost a process.
    if (g_simplify_engine.load(std::memory_order_relaxed) ==
        SimplifyEngine::Libspot) {
        SpotSimplification in_process =
            spot_try_simplify(formula, k_libspot_lock_budget, timeout);
        // Falling back needs something to fall back to. Without the binary the
        // exec path below returns the formula unsimplified, which would make a
        // busy lock silently change the answer, so wait for the lock instead.
        //
        // Waiting in slices rather than blocking outright, because the lock is
        // not always going to come free. A translation that missed its deadline
        // holds it for as long as it keeps running, and with no ltlfilt to
        // spawn there is nothing else to do -- so a plain block here is the one
        // place on this path that can hang a run outright. Re-checking each
        // slice turns that into a failed individual with a message naming the
        // cause, which max_scoring_failure_rate then treats like any other.
        while (in_process.m_lock_busy &&
               access(ltlfilt_path().c_str(), F_OK) != 0) {
            if (spot_abandoned_workers() > 0) {
                throw std::runtime_error(
                    "cannot simplify \"" + formula +
                    "\": libspot is held by an abandoned call, and "
                    "there is no ltlfilt at " +
                    ltlfilt_path() + " to fall back to");
            }
            in_process =
                spot_try_simplify(formula, k_no_fallback_wait_slice, timeout);
        }
        // A timed-out simplification reports neither a formula nor a busy
        // lock, so it falls into the branch below and caches the formula
        // unchanged. That is deliberate rather than incidental: unsimplified
        // is what this function already returns when there is no ltlfilt to
        // run, it is logically the same formula, and sending it to the exec
        // instead would spend the same budget again on a formula already known
        // to exceed it.
        //
        // Busy means another thread is inside libspot and this one would wait
        // longer than a spawn costs, so it spawns instead. Everything below is
        // the exec path, unchanged.
        if (!in_process.m_lock_busy) {
            count_simplify_timeout(in_process);
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            std::scoped_lock lock(cache_mutex);
            LtlfiltStats::total_time_s += elapsed;
            const std::string& simplified =
                in_process.m_formula.value_or(formula);
            cache.emplace(formula, simplified);
            return simplified;
        }
        // Past here the in-process path was skipped and a spawn pays for it.
        // The rate is what says whether the one lock has become the
        // bottleneck: the optimisation turns itself off under contention, and
        // this is the only place that shows it happening.
        profile::add_count("libspot/simplify-lock-busy");
    }
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        std::scoped_lock lock(cache_mutex);
        cache.emplace(formula, formula);
        return formula;
    }
    std::string simplified = formula;
    double child_cpu_s = 0.0;
    // Nothing back means "run it yourself" -- batching off, a formula that
    // cannot be batched, or a batch that failed its line-count check. All three
    // fall through to the one-shot exec below.
    const std::optional<std::string> batched =
        batched_simplify(binary, formula, child_cpu_s, timeout);
    if (batched.has_value()) {
        simplified = *batched;
    } else {
        COUNTER_PROFILE_SCOPE("ltlfilt/one-shot-exec");
        // A killed child exits non-zero, so a timeout lands on the same branch
        // as any other ltlfilt failure and leaves the formula unsimplified --
        // the same answer the in-process path gives when its deadline expires.
        const SubprocessResult result = run_subprocess(
            {binary, "--simplify", "-f", formula}, SubprocessOptions{timeout});
        // Added, not assigned. Reaching here after leading a batch means the
        // batch ran and then failed its line-count check, and its child CPU is
        // already in child_cpu_s; overwriting it would drop a whole exec from
        // the total.
        child_cpu_s += result.m_cpu_s;
        if (result.m_exit_code == 0 && !result.m_output.empty()) {
            simplified = result.m_output;
            while (!simplified.empty() && simplified.back() == '\n') {
                simplified.pop_back();
            }
        }
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    LtlfiltStats::total_cpu_s += child_cpu_s;
    cache.emplace(formula, simplified);
    return simplified;
}

std::string normalize_ltl(const std::string& formula) {
    std::string simplified = simplify_ltl(formula);
    // SPOT uses "0"/"1" for the boolean constants false/true. There is no
    // single keyword accepted by all downstream tools (black treats "false" as
    // an atom, not a constant), so fall back to the original formula in these
    // cases to preserve correctness.
    if (simplified == "0" || simplified == "1") {
        return formula;
    }
    return simplified;
}

bool ltl_equivalent(const std::string& lhs, const std::string& rhs) {
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return true;
    }
    const SubprocessResult result =
        run_subprocess({binary, "--equivalent-to=" + rhs, "-f", lhs});
    // ltlfilt's filter convention: exit 0 means the input formula (lhs)
    // matched (i.e. is equivalent to rhs); exit 1 means it didn't. Any other
    // status (parse error, crash) is inconclusive, not a mismatch.
    if (result.m_exit_code == 0) {
        return true;
    }
    if (result.m_exit_code == 1) {
        return false;
    }
    return true;
}
