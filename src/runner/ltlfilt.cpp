#include "runner/ltlfilt.hpp"

#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
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
// conjunctions the search builds (35e1467, PR #24), and both callers below
// degrade gracefully when they get no answer, so a bounded wait costs only the
// wait itself.
std::atomic<std::int64_t> g_ltlfilt_timeout_ms{10'000};

// Whether `chr` can sit inside an atom name, and so cannot be an operator
// boundary. Mirrors the identifier rule the propositional parser uses.
bool is_identifier_char(char chr) {
    return (std::isalnum(static_cast<unsigned char>(chr)) != 0) || chr == '_';
}

// Guards every mutable static in this file: both memo caches below and the
// LtlfiltStats counters. One lock rather than one per cache, because the
// counters are shared across all three entry points -- a per-cache lock leaves
// them written under two different mutexes, which is a race whatever each
// lock's own cache is doing. Held only around map lookups and counter
// arithmetic; every ltlfilt exec runs outside it, so the serialisation costs
// nothing against a subprocess spawn.
std::mutex g_ltlfilt_mutex;

std::chrono::milliseconds ltlfilt_timeout() {
    return std::chrono::milliseconds(g_ltlfilt_timeout_ms.load());
}

struct OneShotSimplify {
    std::string m_formula;
    double m_cpu_s = 0.0;
    bool m_timed_out = false;
};

// The formula-at-a-time exec behind every simplification. A killed child exits
// non-zero, so a timeout lands on the same branch as any other ltlfilt failure
// and leaves the formula unsimplified.
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
    {
        COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl:cache-lookup");
        std::scoped_lock lock(g_ltlfilt_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            LtlfiltStats::n_cache_hits++;
            return found->second;
        }
        LtlfiltStats::n_cache_misses++;
    }
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        std::scoped_lock lock(g_ltlfilt_mutex);
        cache.emplace(formula, formula);
        return formula;
    }
    const auto start = std::chrono::steady_clock::now();
    const OneShotSimplify one_shot = one_shot_simplify(binary, formula);
    const std::string simplified = one_shot.m_formula;
    const double child_cpu_s = one_shot.m_cpu_s;
    const bool timed_out = one_shot.m_timed_out;
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(g_ltlfilt_mutex);
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

bool has_weak_operator(const std::string& formula) {
    for (std::size_t pos = 0; pos < formula.size(); ++pos) {
        if (formula[pos] != 'W' && formula[pos] != 'M') {
            continue;
        }
        const bool joined_left =
            pos > 0 && is_identifier_char(formula[pos - 1]);
        const bool joined_right =
            pos + 1 < formula.size() && is_identifier_char(formula[pos + 1]);
        if (!joined_left && !joined_right) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> rewrite_weak_operators(const std::string& formula) {
    COUNTER_PROFILE_SCOPE("ltlfilt/rewrite_weak_operators");
    static std::unordered_map<std::string, std::optional<std::string>> cache;
    {
        std::scoped_lock lock(g_ltlfilt_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            LtlfiltStats::n_cache_hits++;
            return found->second;
        }
        LtlfiltStats::n_cache_misses++;
    }
    const auto remember = [&formula](std::optional<std::string> result) {
        std::scoped_lock lock(g_ltlfilt_mutex);
        // Failures are cached like successes, as in simplify_ltl: a formula
        // ltlfilt cannot rewrite once it cannot rewrite ever, and a timeout
        // that blew the budget once will blow it again.
        cache.emplace(formula, result);
        return result;
    };
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return remember(std::nullopt);
    }
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(
        {binary, "--remove-wm", "-p", "-f", formula}, ltlfilt_timeout());
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    {
        std::scoped_lock lock(g_ltlfilt_mutex);
        LtlfiltStats::total_time_s += elapsed;
        LtlfiltStats::total_cpu_s += result.m_cpu_s;
        if (result.m_timed_out) {
            LtlfiltStats::n_timeouts++;
        }
    }
    if (result.m_exit_code != 0 || result.m_output.empty()) {
        return remember(std::nullopt);
    }
    std::string rewritten = result.m_output;
    while (!rewritten.empty() && rewritten.back() == '\n') {
        rewritten.pop_back();
    }
    // --remove-wm is defined to eliminate both operators, so a survivor means
    // the rewrite did not do what its caller is relying on. Report no answer
    // rather than pass it on.
    if (rewritten.empty() || has_weak_operator(rewritten)) {
        return remember(std::nullopt);
    }
    return remember(rewritten);
}

bool ltl_equivalent(const std::string& lhs, const std::string& rhs) {
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return true;
    }
    const ProcessResult result = execute_and_capture(
        {binary, "--equivalent-to=" + rhs, "-f", lhs}, ltlfilt_timeout());
    if (result.m_timed_out) {
        std::scoped_lock lock(g_ltlfilt_mutex);
        LtlfiltStats::n_timeouts++;
    }
    // ltlfilt's filter convention: exit 0 means the input formula (lhs)
    // matched (i.e. is equivalent to rhs); exit 1 means it didn't. Only exit 1
    // is a mismatch — any other status (parse error, crash, the SIGKILL from a
    // timeout) is inconclusive, and reported as equivalent rather than as a
    // false mismatch.
    return result.m_exit_code != 1;
}
