#include "runner/black.hpp"

#include <unistd.h>

#include <array>
#include <cassert>
#include <cctype>
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runner/process.hpp"

namespace {

bool is_identifier_char(char chr) {
    return std::isalnum(static_cast<unsigned char>(chr)) != 0 || chr == '_';
}

// This codebase spells its boolean constants as atoms named "true"/"false"
// (see prop_formula.hpp). black parses those bare identifiers as free
// variables, so it reports e.g. "G(false)" satisfiable by holding the variable
// forever. It does read "True"/"False" as genuine constants, so rewrite whole
// tokens on the way in. This is the only thing keeping black correct about
// constants: check_satisfiability no longer runs an ltlfilt pre-pass that used
// to fold some of them away before black saw them. SPOT needs no equivalent —
// ltlfilt and ltl2tgba already treat "true"/"false" as constants.
std::string to_black_constants(const std::string& formula) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>,
                                2>
        k_constants{{{"true", "True"}, {"false", "False"}}};
    std::string out;
    out.reserve(formula.size());
    std::size_t pos = 0;
    while (pos < formula.size()) {
        bool rewritten = false;
        if (pos == 0 || !is_identifier_char(formula[pos - 1])) {
            for (const auto& [atom, constant] : k_constants) {
                const std::size_t end = pos + atom.size();
                if (formula.compare(pos, atom.size(), atom) != 0 ||
                    (end < formula.size() &&
                     is_identifier_char(formula[end]))) {
                    continue;
                }
                out.append(constant);
                pos = end;
                rewritten = true;
                break;
            }
        }
        if (!rewritten) {
            out.push_back(formula[pos]);
            ++pos;
        }
    }
    return out;
}

}  // namespace

SatisfiabilityChecker& global_sat_checker() {
    static SatisfiabilityChecker instance;
    return instance;
}

std::string black_executable_path() {
#ifdef BLACK_EXECUTABLE_PATH
    return BLACK_EXECUTABLE_PATH;
#else
    assert(false);
    return "";
#endif
}

std::optional<bool> SatisfiabilityChecker::check_satisfiability(
    const std::string& ltl_formula) {
    // No ltlfilt --simplify pre-pass, matching run_ltl2tgba_for_counting and
    // check_realizability_ltl. It was kept here longer than on the SPOT paths
    // for two reasons, and measurement retired both.
    //
    // Canonicalising the cache key does work — over 13868 distinct formulae
    // from fsm and fsm-timing runs it merged them onto 7408 normal forms, a
    // 46.6% collapse — but not hard enough to pay for itself: ltlfilt averages
    // 10.3ms against black's 19.1ms on the same corpus, so the 6462 black calls
    // it avoids fall short of the 7483 needed to break even. Dropping it is
    // about 7% cheaper on this path, and removes the super-exponential
    // ltlfilt --simplify blowup (see 35e1467) from the last path that could
    // still hit it.
    //
    // The constant-fold it also provided claimed to skip "the bulk of what
    // black would otherwise time out on". Measured over 200 sampled formulae
    // per bucket, that is not so: those folding to "0" cost black 19.1ms (200
    // UNSAT) and to "1" 18.7ms (200 SAT), against 19.4ms for formulae that do
    // not fold at all, with no timeouts anywhere. black decides them as fast as
    // anything else, and to_black_constants already keeps it correct on them.
    {
        std::shared_lock lock(m_cache_mutex);
        const auto found = m_cache.find(ltl_formula);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
    }
    n_cache_misses++;
    const std::string black = black_executable_path();
    assert(access(black.c_str(), F_OK) == 0);
    // Round up: black's -t takes whole seconds, and truncating turned any
    // sub-second budget into "-t 0", which disables black's own timeout and
    // left the outer deadline as the only one. Rounding up also keeps the
    // inner timeout the looser of the two, so black still gets its chance to
    // answer "UNKNOWN" cleanly before the outer deadline SIGKILLs it. A zero
    // budget stays zero, meaning no timeout on either side.
    const auto timeout_s =
        std::chrono::ceil<std::chrono::seconds>(m_timeout).count();
    // The formula reaches black exactly as constructed, which is also the cache
    // key now that nothing normalises it first. That is what black needs: it
    // parses SPOT's compact operator notation ("GFa", "a W b") but not every
    // token SPOT can emit — "0"/"1" are a syntax error and "xor" is
    // unsupported — whereas a formula from requirement_to_ltl or an implication
    // check is always black-compatible.
    const std::vector<std::string> command = {
        black, "solve",
        "-t",  std::to_string(timeout_s),
        "-f",  to_black_constants(ltl_formula)};
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(command, m_timeout);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    total_cpu_s += result.m_cpu_s;
    if (result.m_timed_out) {
        n_timeouts++;
        m_cache.emplace(ltl_formula, std::nullopt);
        return std::nullopt;
    }
    // Check UNSAT before SAT: the former contains the latter as a substring.
    bool sat = false;
    if (result.m_output.find("UNSAT") != std::string::npos) {
        sat = false;
    } else if (result.m_output.find("SAT") != std::string::npos) {
        sat = true;
    } else if (result.m_output.find("UNKNOWN") != std::string::npos) {
        // black's own internal timeout fired before our outer deadline: it
        // exited normally with "UNKNOWN (stopped at k = N)".  Treat as
        // indeterminate, same as a process-level timeout.
        n_timeouts++;
        m_cache.emplace(ltl_formula, std::nullopt);
        return std::nullopt;
    } else {
        // black's output crossed a process boundary and didn't match any
        // expected form: don't let assert() (a no-op in release builds) fall
        // through to __builtin_unreachable(), which is undefined behavior if
        // this branch is ever actually taken.
        throw std::runtime_error("unexpected output from black: " +
                                 result.m_output);
    }
    m_cache.emplace(ltl_formula, sat);
    return sat;
}
