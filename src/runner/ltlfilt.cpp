#include "runner/ltlfilt.hpp"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "profile.hpp"
#include "runner/process.hpp"
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
    const ProcessResult result = execute_and_capture(
        {binary, "--simplify", "-f", formula}, ltlfilt_timeout());
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::string simplified = formula;
    if (result.m_exit_code == 0 && !result.m_output.empty()) {
        simplified = result.m_output;
        while (!simplified.empty() && simplified.back() == '\n') {
            simplified.pop_back();
        }
    }
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    LtlfiltStats::total_cpu_s += result.m_cpu_s;
    if (result.m_timed_out) {
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
