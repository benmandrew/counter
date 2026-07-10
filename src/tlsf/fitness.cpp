#include "tlsf/fitness.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fitness/halstead.hpp"
#include "fitness/model_counter.hpp"
#include "fitness/transfer_matrix.hpp"
#include "prop_formula.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

namespace {

using Section = std::vector<Formula>;

const Section* all_sections(const tlsf::Specification& spec, std::size_t idx) {
    switch (idx) {
        case 0:
            return &spec.m_initially;
        case 1:
            return &spec.m_preset;
        case 2:
            return &spec.m_require;
        case 3:
            return &spec.m_assume;
        case 4:
            return &spec.m_assert;
        default:
            return &spec.m_guarantee;
    }
}

constexpr std::size_t k_n_sections = 6;

void collect_atoms(const Formula& formula, std::set<std::string>& out) {
    switch (formula.kind()) {
        case Formula::Kind::Atom: {
            const auto name = formula.atom_name();
            if (name.has_value()) {
                out.insert(*name);
            }
            break;
        }
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            const auto child = formula.unary_child();
            if (child.has_value()) {
                collect_atoms(*child, out);
            }
            break;
        }
        default: {
            const auto children = formula.binary_children();
            if (children.has_value()) {
                collect_atoms(children->first, out);
                collect_atoms(children->second, out);
            }
            break;
        }
    }
}

// Semantic similarity of a single (changed) formula pair: the harmonic mean of
// the two directional bounded-trace containment ratios, mirroring the FRETISH
// requirement-pair routine in semantic_similarity.cpp. All three systems share
// one atom universe (the union of both formulae's atoms) so the conjunction
// count never exceeds either individual count.
double formula_pair_semantic_similarity(const Formula& first,
                                        const Formula& second,
                                        std::size_t bound) {
    std::set<std::string> atoms;
    collect_atoms(first, atoms);
    collect_atoms(second, atoms);
    const std::size_t n_atoms = atoms.size();
    if (n_atoms == 0) {
        return first == second ? 1.0 : 0.0;
    }
    const std::string ltl_first = first.to_string();
    const std::string ltl_second = second.to_string();
    const std::string conjunction =
        "(" + ltl_first + ") & (" + ltl_second + ")";
    const Count count_first =
        count_traces(build_transfer_system_from_ltl(ltl_first, n_atoms), bound);
    const Count count_second = count_traces(
        build_transfer_system_from_ltl(ltl_second, n_atoms), bound);
    const Count count_conjunction = count_traces(
        build_transfer_system_from_ltl(conjunction, n_atoms), bound);
    if (count_first == 0 && count_second == 0) {
        return 1.0;
    }
    if (count_first == 0 || count_second == 0) {
        return 0.0;
    }
    const auto forward =
        static_cast<double>(static_cast<long double>(count_conjunction) /
                            static_cast<long double>(count_first));
    const auto backward =
        static_cast<double>(static_cast<long double>(count_conjunction) /
                            static_cast<long double>(count_second));
    if (forward == 0.0 && backward == 0.0) {
        return 0.0;
    }
    return (2.0 * forward * backward) / (forward + backward);
}

HalsteadCounts merge_counts(HalsteadCounts lhs, const HalsteadCounts& rhs) {
    lhs.eta1 += rhs.eta1;
    lhs.eta2 += rhs.eta2;
    lhs.n1 += rhs.n1;
    lhs.n2 += rhs.n2;
    return lhs;
}

HalsteadCounts spec_halstead_counts(const tlsf::Specification& spec) {
    HalsteadCounts total;
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        for (const Formula& formula : *all_sections(spec, section)) {
            total = merge_counts(total, halstead_counts(formula));
        }
    }
    return total;
}

}  // namespace

double tlsf_syntactic_similarity(const tlsf::Specification& spec,
                                 const tlsf::Specification& original,
                                 [[maybe_unused]] const Config& cfg) {
    double total = 0.0;
    std::size_t n_pairs = 0;
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        const Section& lhs = *all_sections(spec, section);
        const Section& rhs = *all_sections(original, section);
        const std::size_t paired = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < paired; ++i) {
            total += lhs[i].syntactic_similarity(rhs[i]);
        }
        // Missing pairs (the size difference) contribute similarity 0.
        n_pairs += std::max(lhs.size(), rhs.size());
    }
    if (n_pairs == 0) {
        return 1.0;
    }
    return total / static_cast<double>(n_pairs);
}

double tlsf_semantic_similarity(const tlsf::Specification& spec,
                                const tlsf::Specification& original,
                                const Config& cfg) {
    const std::size_t bound = cfg.default_model_counting_bound;
    double total = 0.0;
    std::size_t changed = 0;
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        const Section& lhs = *all_sections(spec, section);
        const Section& rhs = *all_sections(original, section);
        const std::size_t paired = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < paired; ++i) {
            if (lhs[i] == rhs[i]) {
                continue;
            }
            total += formula_pair_semantic_similarity(lhs[i], rhs[i], bound);
            ++changed;
        }
    }
    if (changed == 0) {
        return 1.0;
    }
    return total / static_cast<double>(changed);
}

double tlsf_halstead_fitness(const tlsf::Specification& spec,
                             const tlsf::Specification& original,
                             [[maybe_unused]] const Config& cfg) {
    const double volume = halstead_volume(spec_halstead_counts(spec));
    const double original_volume =
        halstead_volume(spec_halstead_counts(original));
    if (volume <= 0.0) {
        return 1.0;
    }
    if (original_volume <= 0.0) {
        return 0.0;
    }
    return std::min(1.0, original_volume / volume);
}

double tlsf_status(const tlsf::Specification& spec,
                   [[maybe_unused]] const Config& cfg) {
    SatisfiabilityChecker& sat = global_sat_checker();
    RealizabilityChecker& real = global_real_checker();
    auto all_sat = [&sat](const Section& section) {
        return std::all_of(
            section.begin(), section.end(), [&sat](const Formula& formula) {
                return sat.check_satisfiability(formula.to_string())
                    .value_or(false);
            });
    };
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        if (!all_sat(*all_sections(spec, section))) {
            return 0.0;
        }
    }
    if (!sat.check_satisfiability(spec.guarantee_ltl()).value_or(false)) {
        return 0.1;
    }
    if (!sat.check_satisfiability(spec.assumption_ltl()).value_or(false)) {
        return 0.2;
    }
    if (!real.check_realizability_ltl(spec.to_ltl(), spec.m_inputs,
                                      spec.m_outputs)) {
        return 0.5;
    }
    return 1.0;
}

AggregateWeightedFitnessFunctionT<tlsf::Specification>
tlsf_get_fitness_function(const tlsf::Specification& original,
                          const Config& cfg) {
    std::vector<WeightedFitnessFunctionT<tlsf::Specification>> functions;
    if (cfg.fitness_weight_syntactic > 0.0) {
        functions.push_back({[original, cfg](const tlsf::Specification& spec) {
                                 return tlsf_syntactic_similarity(
                                     spec, original, cfg);
                             },
                             cfg.fitness_weight_syntactic, "syntactic"});
    }
    if (cfg.fitness_weight_semantic > 0.0) {
        functions.push_back({[original, cfg](const tlsf::Specification& spec) {
                                 return tlsf_semantic_similarity(spec, original,
                                                                 cfg);
                             },
                             cfg.fitness_weight_semantic, "semantic"});
    }
    if (cfg.fitness_weight_halstead > 0.0) {
        functions.push_back({[original, cfg](const tlsf::Specification& spec) {
                                 return tlsf_halstead_fitness(spec, original,
                                                              cfg);
                             },
                             cfg.fitness_weight_halstead, "halstead"});
    }
    if (cfg.fitness_weight_status > 0.0) {
        functions.push_back({[cfg](const tlsf::Specification& spec) {
                                 return tlsf_status(spec, cfg);
                             },
                             cfg.fitness_weight_status, "status"});
    }
    return AggregateWeightedFitnessFunctionT<tlsf::Specification>(
        std::move(functions));
}
