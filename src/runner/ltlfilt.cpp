#include "runner/ltlfilt.hpp"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "profile.hpp"
#include "runner/process.hpp"
#include "runner/simplify_batcher.hpp"
#include "runner/spot.hpp"

namespace {

// Per-call wall-clock budget for the ltlfilt exec, in milliseconds; 0 disables
// it. Set once at startup from Config::ltlfilt_timeout, read by every worker,
// hence atomic. The initial value mirrors that config default, for the callers
// that never load a config (the tests and the mucs/ltl tools).
//
// Unlike the ltlsynt and ltl2tgba budgets this defaults to a real value rather
// than to "off": --simplify is super-exponential on the deep nested-X
// conjunctions the search builds (see 35e1467), and both callers below already
// degrade gracefully when they get no answer, so a bounded wait costs only the
// wait itself.
std::atomic<std::int64_t> g_ltlfilt_timeout_ms{10'000};

std::chrono::milliseconds ltlfilt_timeout() {
    return std::chrono::milliseconds(g_ltlfilt_timeout_ms.load());
}

struct OneShotSimplify {
    std::string m_formula;
    double m_cpu_s = 0.0;
    bool m_timed_out = false;
};

// The formula-at-a-time exec, unchanged from before batching existed: what
// simplify_ltl falls back to whenever the batcher declines a formula. A killed
// child exits non-zero, so a timeout lands on the same branch as any other
// ltlfilt failure and leaves the formula unsimplified.
OneShotSimplify one_shot_simplify(const std::string& binary,
                                  const std::string& formula) {
    COUNTER_PROFILE_SCOPE("ltlfilt/one-shot-exec");
    const ProcessResult result = execute_and_capture(
        {binary, "--simplify", "-f", formula}, ltlfilt_timeout());
    OneShotSimplify out{formula, result.m_cpu_s, result.m_timed_out};
    if (result.m_exit_code == 0 && !result.m_output.empty()) {
        out.m_formula = result.m_output;
        while (!out.m_formula.empty() && out.m_formula.back() == '\n') {
            out.m_formula.pop_back();
        }
    }
    return out;
}

}  // namespace

void set_ltlfilt_timeout(std::chrono::milliseconds timeout) {
    g_ltlfilt_timeout_ms.store(timeout.count());
}

std::string ltlfilt_path() { return spot_bin_dir() + "/ltlfilt"; }

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
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        std::scoped_lock lock(cache_mutex);
        cache.emplace(formula, formula);
        return formula;
    }
    const auto start = std::chrono::steady_clock::now();
    std::string simplified;
    double child_cpu_s = 0.0;
    bool timed_out = false;
    // Nothing back means "run it yourself" -- batching off, a formula that
    // cannot be batched, or a batch that failed its line-count check. All three
    // fall through to the one-shot exec.
    const std::optional<std::string> batched =
        batched_simplify(binary, formula, child_cpu_s, ltlfilt_timeout());
    if (batched.has_value()) {
        simplified = *batched;
    } else {
        const OneShotSimplify one_shot = one_shot_simplify(binary, formula);
        simplified = one_shot.m_formula;
        // Added, not assigned. Reaching here after leading a batch means the
        // batch ran and then failed its line-count check, and its child CPU is
        // already in child_cpu_s; overwriting it would drop a whole exec from
        // the total.
        child_cpu_s += one_shot.m_cpu_s;
        timed_out = one_shot.m_timed_out;
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    LtlfiltStats::total_cpu_s += child_cpu_s;
    if (timed_out) {
        LtlfiltStats::n_timeouts++;
    }
    // The unsimplified fallback is cached like any other result, including
    // after a timeout: a formula that blew the budget once will blow it every
    // time, and re-paying the wait per occurrence is the stall this timeout
    // exists to avoid.
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
    const ProcessResult result = execute_and_capture(
        {binary, "--equivalent-to=" + rhs, "-f", lhs}, ltlfilt_timeout());
    if (result.m_timed_out) {
        LtlfiltStats::n_timeouts++;
    }
    // ltlfilt's filter convention: exit 0 means the input formula (lhs)
    // matched (i.e. is equivalent to rhs); exit 1 means it didn't. Only exit 1
    // is a mismatch — any other status (parse error, crash, the SIGKILL from a
    // timeout) is inconclusive, and reported as equivalent rather than as a
    // false mismatch.
    return result.m_exit_code != 1;
}
