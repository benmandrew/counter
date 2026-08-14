#include "runner/spot.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "requirement.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/process.hpp"

namespace {

// Process-global counting gate limiting concurrent ltlsynt executions. C++17
// has no counting_semaphore, so this is a mutex + condition_variable. A limit
// of 0 means unlimited, so acquire()/release() are no-ops in that case and the
// pre-cap behaviour is preserved exactly.
class ConcurrencyGate {
   public:
    void set_limit(std::size_t limit) {
        {
            const std::scoped_lock lock(m_mutex);
            m_limit = limit;
        }
        m_cv.notify_all();
    }
    void acquire() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_limit == 0 || m_active < m_limit; });
        ++m_active;
    }
    void release() {
        {
            const std::scoped_lock lock(m_mutex);
            --m_active;
        }
        m_cv.notify_one();
    }

   private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::size_t m_limit = 0;
    std::size_t m_active = 0;
};

ConcurrencyGate& ltlsynt_gate() {
    static ConcurrencyGate gate;
    return gate;
}

// Per-call wall-clock budget for the ltlsynt exec, in milliseconds; 0 disables
// the timeout. Set once at startup from Config::ltlsynt_timeout, read by every
// worker, hence atomic. ltlsynt normally decides in milliseconds, but hard
// synthesis queries the genetic search stumbles onto can run for minutes with
// no upper bound, so an unbounded run stalls on the tail.
std::atomic<std::int64_t> g_ltlsynt_timeout_ms{0};

// Per-call wall-clock budget for the ltl2tgba counting exec, in milliseconds; 0
// disables the timeout. Set once at startup from Config::ltl2tgba_timeout, read
// by every scoring worker, hence atomic. Mirrors g_ltlsynt_timeout_ms: ltl2tgba
// has no internal timeout and its -D determinization can run unbounded on the
// deeply nested formulae the search builds, orphaning a multi-GB process when
// the run is torn down.
std::atomic<std::int64_t> g_ltl2tgba_timeout_ms{0};

// RAII acquire/release around the ltlsynt exec, so a throwing execute call
// (or parse) never leaks a permit and deadlocks the remaining workers.
class GateGuard {
   public:
    GateGuard() { ltlsynt_gate().acquire(); }
    ~GateGuard() { ltlsynt_gate().release(); }
    GateGuard(const GateGuard&) = delete;
    GateGuard& operator=(const GateGuard&) = delete;
};

std::string join_comma(const std::vector<std::string>& items) {
    std::string result;
    bool first = true;
    for (const auto& item : items) {
        if (!first) {
            result += ',';
        }
        result += item;
        first = false;
    }
    return result;
}

// Removed requirements are skipped: they are not part of what the
// specification says, so they must not reach a solver.
void build_ltl_conjunction(const std::vector<Requirement>& reqs,
                           std::string& out) {
    bool first = true;
    for (const Requirement& req : reqs) {
        if (req.m_removed) {
            continue;
        }
        if (!first) {
            out += " & ";
        }
        out += "(" + req.m_ltl + ")";
        first = false;
    }
    // An all-removed list conjoins to nothing. Emit the unit of conjunction
    // rather than an empty string, which would produce `() -> ()`.
    if (out.empty()) {
        out = "true";
    }
}

void build_specification_formula(const Specification& specification,
                                 std::string& formula) {
    if (count_live(specification.m_assumptions) == 0) {
        build_ltl_conjunction(specification.m_guarantees, formula);
        return;
    }
    std::string conj_a;
    build_ltl_conjunction(specification.m_assumptions, conj_a);
    std::string conj_g;
    build_ltl_conjunction(specification.m_guarantees, conj_g);
    formula = "(" + conj_a + ") -> (" + conj_g + ")";
}

// The universal (trivially-true) automaton, in the exact HOA shape ltl2tgba
// itself emits for the constant `true`: one accepting state over zero atoms
// with a `[t]` self-loop. Substituted for the exit-2-on-tautology bug below;
// parses to the same TransferSystem a genuine tautology would.
constexpr const char* k_universal_hoa =
    "HOA: v1\n"
    "name: \"1\"\n"
    "States: 1\n"
    "Start: 0\n"
    "AP: 0\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "properties: trans-labels explicit-labels state-acc complete\n"
    "properties: deterministic stutter-invariant weak\n"
    "--BODY--\n"
    "State: 0\n"
    "[t] 0\n"
    "--END--\n";

// SPOT 2.15.1's ltl2tgba aborts with exit 2 and this stderr line when a formula
// reduces to a tautology (the printed automaton is universal, hence complete,
// but its prop_complete() flag was left unset). The signature is stable across
// the invocation's binary-path prefix.
bool is_tautology_print_error(const ProcessResult& result) {
    return result.m_exit_code == 2 &&
           result.m_output.find("automaton is complete but prop_complete()") !=
               std::string::npos;
}

bool parse_realizability_output(const ProcessResult& result) {
    if (result.m_output.find("UNREALIZABLE") != std::string::npos) {
        return false;
    }
    if (result.m_output.find("REALIZABLE") != std::string::npos) {
        return true;
    }
    // ltlsynt's output crossed a process boundary and didn't match either
    // expected form: don't let assert() (a no-op in release builds) treat
    // this as UNREALIZABLE and cache a fabricated result.
    throw std::runtime_error("unrecognized ltlsynt output: " + result.m_output);
}

}  // namespace

RealizabilityChecker& global_real_checker() {
    static RealizabilityChecker instance;
    return instance;
}

std::string spot_bin_dir() {
#ifdef SPOT_BIN_DIR
    return SPOT_BIN_DIR;
#else
    assert(false);
    return "";
#endif
}

std::string ltlsynt_path() { return spot_bin_dir() + "/ltlsynt"; }

void RealizabilityChecker::set_max_concurrency(std::size_t limit) {
    ltlsynt_gate().set_limit(limit);
}

void RealizabilityChecker::set_timeout(std::chrono::milliseconds timeout) {
    g_ltlsynt_timeout_ms.store(timeout.count());
}

void set_ltl2tgba_timeout(std::chrono::milliseconds timeout) {
    g_ltl2tgba_timeout_ms.store(timeout.count());
}

std::string ltl2tgba_path() { return spot_bin_dir() + "/ltl2tgba"; }

std::string run_ltl2tgba_for_counting(const std::string& formula) {
    // No normalize_ltl() pre-pass here, unlike the other SPOT/black callers.
    // ltl2tgba simplifies internally, so it is redundant -- and ltlfilt
    // --simplify blows up super-exponentially on the deeply nested-X
    // conjunctions this path builds for atom-rich, deep-horizon requirement
    // pairs (e.g. WithinTicks(20) over ~10 atoms): ~1s at 12 ticks, ~21s at
    // 15, unbounded by 20, where ltl2tgba yields the identical automaton from
    // the raw formula in milliseconds. Passing the formula straight through
    // avoids that cliff.
    static std::unordered_map<std::string, std::string> cache;
    // Formulae whose determinization was abandoned at the budget. Memoised
    // like the automata themselves: the blowup is a property of the formula,
    // not of the moment, so re-running it buys the same timeout at full price
    // and the individual is dropped either way.
    static std::unordered_set<std::string> timed_out;
    static std::mutex cache_mutex;
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            Ltl2tgbaStats::n_cache_hits++;
            return found->second;
        }
        if (timed_out.count(formula) != 0) {
            // Same error as the exec would have raised, so a caller sees one
            // behaviour whether or not this formula has been tried before.
            Ltl2tgbaStats::n_cache_hits++;
            throw std::runtime_error("ltl2tgba timed out for formula: " +
                                     formula);
        }
        Ltl2tgbaStats::n_cache_misses++;
    }
    const std::string binary = ltl2tgba_path();
    assert(access(binary.c_str(), F_OK) == 0);
    const auto start = std::chrono::steady_clock::now();
    const auto timeout =
        std::chrono::milliseconds(g_ltl2tgba_timeout_ms.load());
    const ProcessResult result =
        execute_and_capture({binary, "-D", "-S", "-H", "-f", formula}, timeout);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (result.m_timed_out) {
        // The determinization did not finish within budget; drop the individual
        // (counted against max_scoring_failure_rate) rather than caching a
        // partial result or stalling the run. execute_and_capture has already
        // SIGKILLed and reaped the child, so no process is left behind.
        {
            std::scoped_lock lock(cache_mutex);
            Ltl2tgbaStats::record_time(elapsed, result.m_cpu_s);
            Ltl2tgbaStats::n_timeouts++;
            timed_out.insert(formula);
        }
        throw std::runtime_error("ltl2tgba timed out for formula: " + formula);
    }
    if (is_tautology_print_error(result)) {
        // The formula is a tautology: it accepts every trace, so the universal
        // automaton is the correct result, not a scoring failure. Substituting
        // it (rather than letting the throw drop the individual) keeps a
        // genuinely-true formula from counting against the run's
        // max_scoring_failure_rate tolerance.
        std::scoped_lock lock(cache_mutex);
        Ltl2tgbaStats::record_time(elapsed, result.m_cpu_s);
        Ltl2tgbaStats::n_tautology_substitutions++;
        cache.emplace(formula, k_universal_hoa);
        return k_universal_hoa;
    }
    if (result.m_exit_code != 0) {
        // A non-zero exit here (e.g. a subprocess spawn failure under heavy
        // concurrent forking) must not be cached as a successful result
        throw std::runtime_error("ltl2tgba exited with code " +
                                 std::to_string(result.m_exit_code) +
                                 " for formula: " + formula);
    }
    std::scoped_lock lock(cache_mutex);
    Ltl2tgbaStats::record_time(elapsed, result.m_cpu_s);
    cache.emplace(formula, result.m_output);
    return result.m_output;
}

std::optional<bool> RealizabilityChecker::check_realizability(
    const Specification& specification) {
    std::string conj_ltl;
    build_specification_formula(specification, conj_ltl);
    return check_realizability_ltl(conj_ltl, specification.m_in_atoms,
                                   specification.m_out_atoms);
}

std::optional<bool> RealizabilityChecker::check_realizability_ltl(
    const std::string& ltl_formula, const std::vector<std::string>& inputs,
    const std::vector<std::string>& outputs) {
    // No normalize_ltl() pre-pass, matching run_ltl2tgba_for_counting: ltlsynt
    // simplifies internally, and the specification formula is a conjunction of
    // the guarantees, which reproduces the deeply nested-X shape that hangs
    // ltlfilt --simplify for multi-guarantee deep-horizon specs (e.g. two
    // WithinTicks(20) guarantees with different responses -- reachable via
    // mutate_timing, and re-checked here for every survivor). ltlsynt decides
    // the raw formula in milliseconds, so pass it straight through. Unlike the
    // black path, nothing here depends on the "0"/"1" fold normalize enables.
    const std::string& conj_ltl = ltl_formula;
    const std::string cache_key =
        conj_ltl + "|" + join_comma(inputs) + "|" + join_comma(outputs);
    {
        std::scoped_lock lock(m_cache_mutex);
        const auto found = m_cache.find(cache_key);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
        n_cache_misses++;
    }
    const std::string ltlsynt = ltlsynt_path();
    assert(access(ltlsynt.c_str(), F_OK) == 0);
    std::vector<std::string> command = {ltlsynt, "--realizability", "-f",
                                        conj_ltl};
    if (!inputs.empty()) {
        command.push_back("--ins=" + join_comma(inputs));
    } else if (!outputs.empty()) {
        command.push_back("--outs=" + join_comma(outputs));
    }
    const auto timeout = std::chrono::milliseconds(g_ltlsynt_timeout_ms.load());
    const auto start = std::chrono::steady_clock::now();
    ProcessResult result;
    {
        // Hold a permit only for the exec: the child's multi-GB footprint is
        // freed once execute_and_capture reaps it, so parsing and cache updates
        // run outside the gate.
        const GateGuard gate_guard;
        result = execute_and_capture(command, timeout);
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    // A timed-out query is undecided, not unrealizable. Which of the two is
    // the safe reading depends on the question being asked -- admitting a
    // repair wants "unrealizable", the well-separation filter wants
    // "realizable" -- so the direction is the caller's to pick, and this
    // reports nullopt rather than picking one for everybody.
    const std::optional<bool> realizable =
        result.m_timed_out
            ? std::nullopt
            : std::optional<bool>(parse_realizability_output(result));
    // Diagnostic hook (off unless COUNTER_LTLSYNT_LOG names a file): append one
    // "elapsed_s timed_out n_atoms" line per ltlsynt exec, for studying the
    // call-duration distribution and tuning ltlsynt_timeout. Zero cost when the
    // env var is unset.
    if (const char* log_path = std::getenv("COUNTER_LTLSYNT_LOG")) {
        static std::mutex log_mutex;
        const std::scoped_lock log_lock(log_mutex);
        std::ofstream log_file(log_path, std::ios::app);
        log_file << elapsed << ' ' << (result.m_timed_out ? 1 : 0) << ' '
                 << (inputs.size() + outputs.size()) << '\n';
    }
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    total_cpu_s += result.m_cpu_s;
    if (result.m_timed_out) {
        n_timeouts++;
    }
    // Undecided is memoised like any other outcome, for the reason the ltlfilt
    // cache gives: a formula that blew the budget once will blow it every
    // time, and re-paying that wait per occurrence is the stall the timeout
    // exists to avoid. What #74 had to remove was caching a *verdict* nobody
    // decided; nullopt is not one, so every caller still picks its own
    // direction on every hit.
    m_cache.emplace(cache_key, realizable);
    return realizable;
}

std::string ltlsynt_budget_screen(
    const std::function<std::optional<bool>()>& query,
    std::chrono::milliseconds budget) {
    if (budget.count() <= 0) {
        return {};
    }
    const auto start = std::chrono::steady_clock::now();
    // A raising query is an undecided one, exactly as a timeout is, and is
    // caught for the same reason the input screen catches: this runs before the
    // driver's own handler, so letting it out ends the run with neither a
    // `fatal:` line nor a manifest. The screen is advisory either way -- it
    // times a query the run was about to make anyway -- so the cost of a raise
    // is the timing, not the run.
    std::optional<bool> decided;
    std::string error;
    try {
        decided = query();
    } catch (const std::exception& exc) {
        error = exc.what();
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    BudgetScreen::observed_ms = elapsed.count();
    BudgetScreen::decided = decided.has_value();
    if (!error.empty()) {
        return "warning: the input specification's own realizability query "
               "failed: " +
               error +
               "\n         The status objective cannot be decided for any "
               "candidate this tool rejects the same way.\n";
    }
    if (decided.has_value()) {
        return {};
    }
    // Elapsed is reported rather than the true cost, which is unobservable:
    // the child was killed at the budget, so all this says is "at least".
    return "warning: the input specification's own realizability query did "
           "not finish within runtime.ltlsynt_timeout_ms = " +
           std::to_string(budget.count()) +
           " ms.\n         Every realizability query this run makes is "
           "likely to be abandoned the same way, which leaves the status "
           "objective\n         constant, costs that budget per distinct "
           "candidate, and makes the final gate reject even a correct "
           "repair.\n         Raise runtime.ltlsynt_timeout_ms (0 disables "
           "the budget) if this run is meant to decide realizability.\n";
}
