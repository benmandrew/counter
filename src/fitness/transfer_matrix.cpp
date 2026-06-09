#include "fitness/transfer_matrix.hpp"

#include <cassert>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "prop_formula.hpp"
#include "requirement.hpp"
#include "runner/ganak.hpp"
#include "runner/spot.hpp"

namespace {

// Collects all atom names from a propositional formula tree.
std::set<std::string> collect_formula_atoms(const Formula& formula) {
    std::set<std::string> atoms;
    formula.rewrite_post_order(
        [&](const Formula& sub) -> std::optional<Formula> {
            if (const auto name = sub.atom_name()) {
                if (*name != "true" && *name != "false") {
                    atoms.insert(*name);
                }
            }
            return std::nullopt;
        });
    return atoms;
}

// Returns the number of unique atoms across both formulas.
std::size_t count_requirement_atoms(const Requirement& req) {
    std::set<std::string> atoms = collect_formula_atoms(req.m_trigger);
    const std::set<std::string> resp = collect_formula_atoms(req.m_response);
    atoms.insert(resp.begin(), resp.end());
    return atoms.size();
}

}  // namespace

std::size_t count_joint_atoms(const Requirement& req1,
                              const Requirement& req2) {
    std::set<std::string> atoms = collect_formula_atoms(req1.m_trigger);
    auto ins = [&](const Formula& f) {
        const auto s = collect_formula_atoms(f);
        atoms.insert(s.begin(), s.end());
    };
    ins(req1.m_response);
    ins(req2.m_trigger);
    ins(req2.m_response);
    return atoms.size();
}

namespace {

// Converts a HOA guard label (AP indices) to a propositional formula string
// using the AP names. Digits become AP name references; operators pass through.
std::string hoa_label_to_formula(const std::string& label,
                                 const std::vector<std::string>& aps) {
    if (label == "t") return "true";
    if (label == "f") return "false";
    std::string result;
    std::size_t i = 0;
    while (i < label.size()) {
        const char c = label[i];
        if (c == ' ' || c == '\t') {
            ++i;
            continue;
        }
        if (c == '!' || c == '&' || c == '|' || c == '(' || c == ')') {
            result += c;
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t j = i;
            while (j < label.size() &&
                   std::isdigit(static_cast<unsigned char>(label[j]))) {
                ++j;
            }
            const std::size_t idx = std::stoul(label.substr(i, j - i));
            result += '(' + aps[idx] + ')';
            i = j;
            continue;
        }
        ++i;
    }
    return result;
}

// Counts models of a HOA guard over n_total_atoms variables. SPOT may
// produce automata with fewer APs than the requirement has atoms (e.g., when
// the formula simplifies to a tautology). Ganak only counts over variables
// that appear in the formula, so we multiply by 2 for each free variable.
Count count_guard_models(const std::string& label,
                         const std::vector<std::string>& aps,
                         std::size_t n_total_atoms) {
    if (label == "f") return 0;
    if (label == "t") {
        Count result = 1;
        for (std::size_t i = 0; i < n_total_atoms; ++i) {
            Count doubled = 0;
            [[maybe_unused]] const bool ov =
                count_mul_overflow(result, 2, doubled);
            assert(!ov);
            result = doubled;
        }
        return result;
    }

    // Detect which AP indices appear in the label
    std::vector<bool> mentioned(aps.size(), false);
    for (std::size_t i = 0; i < label.size();) {
        if (std::isdigit(static_cast<unsigned char>(label[i]))) {
            std::size_t j = i;
            while (j < label.size() &&
                   std::isdigit(static_cast<unsigned char>(label[j]))) {
                ++j;
            }
            const std::size_t idx = std::stoul(label.substr(i, j - i));
            if (idx < aps.size()) mentioned[idx] = true;
            i = j;
        } else {
            ++i;
        }
    }

    std::size_t n_mentioned = 0;
    for (const bool m : mentioned) {
        if (m) ++n_mentioned;
    }

    // Ganak counts over the n_mentioned vars in the formula; we multiply by
    // 2^(n_total_atoms - n_mentioned) for all remaining free variables.
    const std::size_t free_count = n_total_atoms - n_mentioned;
    Count count = run_ganak_on_formula(hoa_label_to_formula(label, aps));
    for (std::size_t i = 0; i < free_count; ++i) {
        Count doubled = 0;
        [[maybe_unused]] const bool ov = count_mul_overflow(count, 2, doubled);
        assert(!ov);
        count = doubled;
    }
    return count;
}

struct HoaTransition {
    std::string m_guard_label;  // Boolean formula over AP indices, e.g. "!0&1"
    std::size_t m_destination;  // destination state index in the HOA
};

// Parsed representation of a HOA v1 automaton as produced by ltl2tgba -H.
// Safety automata (acc-name: all) have m_is_buchi == false and an implicit
// dead state for any unlisted transition. Buchi automata carry per-state
// acceptance marks used to populate the final-state mask.
struct HoaAutomaton {
    std::size_t m_n_states = 0;
    std::size_t m_initial_state = 0;
    std::vector<std::string> m_aps;  // AP names in index order
    bool m_is_buchi = false;
    std::vector<bool> m_accepting_states;  // indexed by HOA state ID
    std::vector<std::vector<HoaTransition>>
        m_transitions;  // indexed by source state ID
};

// Parses a HOA v1 automaton string into an HoaAutomaton. Reads the header
// fields States, Start, AP, and acc-name, then collects per-state transitions
// from the --BODY-- section. Accepts both safety (acc-name: all) and Buchi
// (acc-name: Buchi) acceptance conditions.
HoaAutomaton parse_hoa(const std::string& hoa_text) {
    HoaAutomaton result;
    std::istringstream ss(hoa_text);
    std::string line;
    bool in_body = false;
    std::size_t current_state = 0;

    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (!in_body) {
            if (line.rfind("States:", 0) == 0) {
                result.m_n_states = std::stoul(line.substr(7));
                result.m_accepting_states.assign(result.m_n_states, false);
                result.m_transitions.resize(result.m_n_states);
            } else if (line.rfind("Start:", 0) == 0) {
                result.m_initial_state = std::stoul(line.substr(6));
            } else if (line.rfind("AP:", 0) == 0) {
                std::istringstream ap_ss(line.substr(3));
                std::size_t k = 0;
                ap_ss >> k;
                result.m_aps.resize(k);
                for (std::size_t i = 0; i < k; ++i) {
                    ap_ss >> std::quoted(result.m_aps[i]);
                }
            } else if (line.rfind("acc-name:", 0) == 0) {
                result.m_is_buchi = line.find("Buchi") != std::string::npos ||
                                    line.find("buchi") != std::string::npos;
            } else if (line == "--BODY--") {
                in_body = true;
            }
        } else if (line == "--END--") {
            break;
        } else if (line.rfind("State:", 0) == 0) {
            std::istringstream state_ss(line.substr(6));
            state_ss >> current_state;
            if (line.find('{') != std::string::npos) {
                result.m_accepting_states[current_state] = true;
            }
        } else if (!line.empty() && line[0] == '[') {
            const std::size_t end_bracket = line.find(']');
            assert(end_bracket != std::string::npos);
            const std::string guard = line.substr(1, end_bracket - 1);
            std::istringstream dest_ss(line.substr(end_bracket + 1));
            std::size_t destination = 0;
            dest_ss >> destination;
            result.m_transitions[current_state].push_back({guard, destination});
        }
    }
    return result;
}

// Converts a parsed HOA automaton into a weighted TransferSystem. States are
// permuted so that the HOA initial state lands at matrix index 0, matching the
// e_0 start vector used by count_traces. Each matrix entry is the sum of
// count_guard_models over all transitions between those two states, weighted
// over the full n_total_atoms variable universe (not just the HOA APs, since
// SPOT may eliminate atoms when it simplifies a formula to a tautology).
// Safety automata produce an empty final_state_mask (all states accept);
// Buchi automata set the mask from the per-state acceptance flags.
TransferSystem build_transfer_system_from_hoa(const HoaAutomaton& hoa,
                                              std::size_t n_total_atoms) {
    const Eigen::Index n = static_cast<Eigen::Index>(hoa.m_n_states);

    // Permute states so the initial state maps to matrix index 0.
    std::vector<std::size_t> perm(hoa.m_n_states);
    perm[0] = hoa.m_initial_state;
    std::size_t fill = 1;
    for (std::size_t i = 0; i < hoa.m_n_states; ++i) {
        if (i != hoa.m_initial_state) {
            perm[fill++] = i;
        }
    }
    std::vector<std::size_t> inv_perm(hoa.m_n_states);
    for (std::size_t i = 0; i < hoa.m_n_states; ++i) {
        inv_perm[perm[i]] = i;
    }

    CountMatrix matrix(n, n);
    matrix.setZero();

    for (std::size_t hoa_from = 0; hoa_from < hoa.m_n_states; ++hoa_from) {
        const Eigen::Index row = static_cast<Eigen::Index>(inv_perm[hoa_from]);
        for (const HoaTransition& trans : hoa.m_transitions[hoa_from]) {
            const Eigen::Index col =
                static_cast<Eigen::Index>(inv_perm[trans.m_destination]);
            const Count weight = count_guard_models(trans.m_guard_label,
                                                    hoa.m_aps, n_total_atoms);
            Count updated = 0;
            [[maybe_unused]] const bool overflow =
                count_add_overflow(matrix(row, col), weight, updated);
            assert(!overflow);
            matrix(row, col) = updated;
        }
    }

    CountVector final_mask;
    if (hoa.m_is_buchi) {
        final_mask.resize(n);
        for (std::size_t i = 0; i < hoa.m_n_states; ++i) {
            final_mask(static_cast<Eigen::Index>(inv_perm[i])) =
                hoa.m_accepting_states[i] ? 1 : 0;
        }
    }

    std::vector<State> states(hoa.m_n_states);
    return {states, CountVector(), matrix, true, final_mask};
}

}  // namespace

TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& /*canonical_valuation_counts*/) {
    const std::size_t n_total_atoms = count_requirement_atoms(requirement);
    const std::string ltl = requirement_to_ltl(requirement);
    const std::string hoa_text = run_ltl2tgba_for_counting(ltl);
    const HoaAutomaton hoa = parse_hoa(hoa_text);
    return build_transfer_system_from_hoa(hoa, n_total_atoms);
}

TransferSystem build_transfer_system(const Requirement& requirement,
                                     std::size_t n_total_atoms) {
    const std::string ltl = requirement_to_ltl(requirement);
    const std::string hoa_text = run_ltl2tgba_for_counting(ltl);
    const HoaAutomaton hoa = parse_hoa(hoa_text);
    return build_transfer_system_from_hoa(hoa, n_total_atoms);
}

TransferSystem build_conjunction_transfer_system(
    const Requirement& requirement1, const Requirement& requirement2) {
    const std::size_t n_total_atoms =
        count_joint_atoms(requirement1, requirement2);
    const std::string ltl1 = requirement_to_ltl(requirement1);
    const std::string ltl2 = requirement_to_ltl(requirement2);
    const std::string conj = "(" + ltl1 + ") & (" + ltl2 + ")";
    const std::string hoa_text = run_ltl2tgba_for_counting(conj);
    const HoaAutomaton hoa = parse_hoa(hoa_text);
    return build_transfer_system_from_hoa(hoa, n_total_atoms);
}

CountMatrix weighted_transition_matrix(const TransferSystem& system) {
    if (system.m_transition_matrix_is_weighted) {
        return system.m_transition_matrix;
    }
    CountMatrix weighted = system.m_transition_matrix;
    for (Eigen::Index column = 0; column < weighted.cols(); ++column) {
        for (Eigen::Index row = 0; row < weighted.rows(); ++row) {
            Count updated = 0;
            [[maybe_unused]] const bool overflow =
                count_mul_overflow(weighted(row, column),
                                   system.m_valuation_counts(column), updated);
            assert(!overflow);
            weighted(row, column) = updated;
        }
    }
    return weighted;
}
