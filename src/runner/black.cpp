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

#include "runner/ltlfilt.hpp"
#include "runner/process.hpp"

namespace {

bool is_identifier_char(char chr) {
    return std::isalnum(static_cast<unsigned char>(chr)) != 0 || chr == '_';
}

// This codebase spells its boolean constants as atoms named "true"/"false"
// (see prop_formula.hpp). black parses those bare identifiers as free
// variables, so it reports e.g. "G(false)" satisfiable by holding the variable
// forever. It does read "True"/"False" as genuine constants, so rewrite whole
// tokens on the way in — without this, black is only ever correct about
// constants when ltlfilt happens to fold them away first, which it cannot do
// when the binary is missing or errors. SPOT needs no equivalent: ltlfilt and
// ltl2tgba already treat "true"/"false" as constants.
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
    // --simplify is kept on this path, unlike the ltl2tgba and ltlsynt ones
    // that dropped it: black's inputs are single requirement formulae and
    // implication checks, not the deep nested-X conjunctions that make ltlfilt
    // --simplify blow up super-exponentially.
    const std::string normalised = simplify_ltl(ltl_formula);
    // A formula that SPOT reduces to a boolean constant is already decided:
    // "0" is unsatisfiable, "1" is valid and therefore satisfiable. The
    // genetic algorithm generates these constantly — mostly implication checks
    // that reduce away entirely — and they are the bulk of what black would
    // otherwise time out on, so answering here skips the subprocess. This is
    // an optimisation only: to_black_constants keeps black correct on these
    // formulae by itself if the folding does not fire.
    if (normalised == "0") {
        n_constant_folded++;
        return false;
    }
    if (normalised == "1") {
        n_constant_folded++;
        return true;
    }
    {
        std::shared_lock lock(m_cache_mutex);
        const auto found = m_cache.find(normalised);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
    }
    n_cache_misses++;
    const std::string black = black_executable_path();
    assert(access(black.c_str(), F_OK) == 0);
    // -t is never omitted. black has no default timeout of its own, so an
    // unbounded solve parks a scoring-pool thread for as long as it takes,
    // and the search loses a worker rather than an individual.
    //
    // Round up: black's -t takes whole seconds, and truncating turned any
    // sub-second budget into "-t 0", which disables black's own timeout and
    // left the outer deadline as the only one. Rounding up also keeps the
    // inner timeout the looser of the two, so black still gets its chance to
    // answer "UNKNOWN" cleanly before the outer deadline SIGKILLs it. A zero
    // budget stays zero, which disables both sides at once and leaves nothing
    // bounding the solve; the config default is 1000 ms and no shipped
    // configuration sets zero.
    const auto timeout_s =
        std::chrono::ceil<std::chrono::seconds>(m_timeout).count();
    // Pass ltl_formula (not normalised) to black. black does parse SPOT's
    // compact operator notation ("GFa", "a W b"), but not every token SPOT can
    // emit: "0"/"1" are a syntax error and "xor" is unsupported. The original
    // formula is always otherwise black-compatible because it comes from
    // requirement_to_ltl / implication check construction. The normalised form
    // is the cache key.
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
        m_cache.emplace(normalised, std::nullopt);
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
        m_cache.emplace(normalised, std::nullopt);
        return std::nullopt;
    } else {
        // black's output crossed a process boundary and didn't match any
        // expected form: don't let assert() (a no-op in release builds) fall
        // through to __builtin_unreachable(), which is undefined behavior if
        // this branch is ever actually taken.
        throw std::runtime_error("unexpected output from black: " +
                                 result.m_output);
    }
    m_cache.emplace(normalised, sat);
    return sat;
}
