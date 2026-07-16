#include "tlsf/filter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "prop_formula.hpp"
#include "runner/black.hpp"
#include "thread_pool.hpp"
#include "tlsf/specification.hpp"

namespace {

// All six section vectors of a specification, in a stable order, so bloat and
// other whole-spec scans can iterate every formula uniformly.
std::array<const std::vector<Formula>*, 6> all_sections(
    const tlsf::Specification& spec) {
    return {&spec.m_initially, &spec.m_preset, &spec.m_require,
            &spec.m_assume,    &spec.m_assert, &spec.m_guarantee};
}

std::size_t max_formula_size(const tlsf::Specification& spec) {
    std::size_t max = 0;
    for (const std::vector<Formula>* section : all_sections(spec)) {
        for (const Formula& formula : *section) {
            max = std::max(max, formula.n_subformulae());
        }
    }
    return max;
}

bool any_formula_exceeds(const tlsf::Specification& spec, std::size_t cap) {
    for (const std::vector<Formula>* section : all_sections(spec)) {
        for (const Formula& formula : *section) {
            if (formula.n_subformulae() > cap) {
                return true;
            }
        }
    }
    return false;
}

// Marks whichever of the unordered pair of representative positions {a, b} is
// strictly dominated, if any. Short-circuits once either endpoint is already
// known subsumed.
void check_pair(const std::vector<tlsf::Specification>& pop,
                const std::vector<std::size_t>& representatives,
                std::vector<std::atomic<uint8_t>>& subsumed,
                SatisfiabilityChecker& checker, std::size_t pos_a,
                std::size_t pos_b) {
    if (subsumed[pos_a].load(std::memory_order_relaxed) != 0U ||
        subsumed[pos_b].load(std::memory_order_relaxed) != 0U) {
        return;
    }
    const tlsf::Specification& spec_a = pop[representatives[pos_a]];
    const tlsf::Specification& spec_b = pop[representatives[pos_b]];
    const bool a_implies_b =
        tlsf_spec_implies(spec_a, spec_b, checker).value_or(false);
    const bool b_implies_a =
        tlsf_spec_implies(spec_b, spec_a, checker).value_or(false);
    if (a_implies_b && !b_implies_a) {
        subsumed[pos_b].store(1, std::memory_order_relaxed);
    } else if (b_implies_a && !a_implies_b) {
        subsumed[pos_a].store(1, std::memory_order_relaxed);
    }
}

// Computes subsumed[j] = 1 iff some spec strictly dominates pop[j] (implies it
// without being implied back). Exact duplicates relate identically to every
// other spec (the implication check depends only on the lowered LTL formula),
// so only one representative per group of equal specs is run through the
// pairwise sweep; its verdict is copied to every member afterwards.
std::vector<uint8_t> compute_subsumed(
    const std::vector<tlsf::Specification>& pop, SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress) {
    const std::size_t pop_size = pop.size();
    std::unordered_map<tlsf::Specification, std::size_t> rep_position_of;
    std::vector<std::size_t> representatives;
    std::vector<std::vector<std::size_t>> members;
    for (std::size_t i = 0; i < pop_size; ++i) {
        const auto [iter, inserted] =
            rep_position_of.try_emplace(pop[i], representatives.size());
        if (inserted) {
            representatives.push_back(i);
            members.push_back({i});
        } else {
            members[iter->second].push_back(i);
        }
    }
    const std::size_t n_reps = representatives.size();
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    pairs.reserve(n_reps * (n_reps - 1) / 2);
    for (std::size_t i = 0; i < n_reps; ++i) {
        for (std::size_t j = i + 1; j < n_reps; ++j) {
            pairs.emplace_back(i, j);
        }
    }
    std::vector<std::atomic<uint8_t>> subsumed(n_reps);
    for (auto& flag : subsumed) {
        flag.store(0, std::memory_order_relaxed);
    }
    const std::size_t n_hw = std::thread::hardware_concurrency();
    const std::size_t max_in_flight = n_hw > 0 ? n_hw * 2 : 1;
    std::size_t completed = 0;
    run_bounded_async(
        pairs.size(), max_in_flight,
        [&checker, &pop, &representatives, &subsumed, &pairs](std::size_t idx) {
            const std::size_t pos_a = pairs[idx].first;
            const std::size_t pos_b = pairs[idx].second;
            return global_thread_pool().submit(
                [&checker, &pop, &representatives, &subsumed, pos_a, pos_b] {
                    check_pair(pop, representatives, subsumed, checker, pos_a,
                               pos_b);
                });
        },
        [&on_progress, &completed, total = pairs.size()](std::size_t) {
            if (on_progress) {
                on_progress(++completed, total);
            }
        });
    std::vector<uint8_t> result(pop_size, 0);
    for (std::size_t rep_pos = 0; rep_pos < n_reps; ++rep_pos) {
        const uint8_t status =
            subsumed[rep_pos].load(std::memory_order_relaxed);
        for (const std::size_t idx : members[rep_pos]) {
            result[idx] = status;
        }
    }
    return result;
}

}  // namespace

FilterFunctionT<tlsf::Specification> tlsf_make_dedup_filter() {
    return {"dedup", [](const std::vector<tlsf::Specification>& pop) {
                std::unordered_set<tlsf::Specification> seen;
                seen.reserve(pop.size());
                std::vector<tlsf::Specification> survivors;
                survivors.reserve(pop.size());
                for (const tlsf::Specification& spec : pop) {
                    if (seen.insert(spec).second) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

FilterFunctionT<tlsf::Specification> tlsf_make_assumption_sat_filter() {
    return {"assumption-sat", [](const std::vector<tlsf::Specification>& pop) {
                std::vector<tlsf::Specification> survivors;
                survivors.reserve(pop.size());
                for (const tlsf::Specification& spec : pop) {
                    const bool no_assumptions = spec.m_initially.empty() &&
                                                spec.m_require.empty() &&
                                                spec.m_assume.empty();
                    if (no_assumptions ||
                        global_sat_checker()
                            .check_satisfiability(spec.assumption_ltl())
                            .value_or(true)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

std::optional<bool> tlsf_spec_implies(const tlsf::Specification& from,
                                      const tlsf::Specification& dest,
                                      SatisfiabilityChecker& checker) {
    if (from == dest) {
        return true;
    }
    const std::optional<bool> sat = checker.check_satisfiability(
        "(" + from.to_ltl() + ") & !(" + dest.to_ltl() + ")");
    if (!sat.has_value()) {
        return std::nullopt;
    }
    return !sat.value();
}

FilterFunctionT<tlsf::Specification> tlsf_make_bloat_cap_filter(
    const tlsf::Specification& original, double max_ratio) {
    const std::size_t original_max = max_formula_size(original);
    return {"bloat-cap", [original_max, max_ratio](
                             const std::vector<tlsf::Specification>& pop) {
                if (original_max == 0) {
                    return pop;
                }
                const auto cap = static_cast<std::size_t>(
                    max_ratio * static_cast<double>(original_max));
                std::vector<tlsf::Specification> survivors;
                survivors.reserve(pop.size());
                for (const tlsf::Specification& spec : pop) {
                    if (!any_formula_exceeds(spec, cap)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

FilterFunctionT<tlsf::Specification> tlsf_make_weakening_filter(
    tlsf::Specification original, SatisfiabilityChecker& checker) {
    return {
        "weakening", [original = std::move(original),
                      &checker](const std::vector<tlsf::Specification>& pop) {
            const std::size_t pop_size = pop.size();
            std::vector<std::atomic<uint8_t>> keep(pop_size);
            for (auto& flag : keep) {
                flag.store(0, std::memory_order_relaxed);
            }
            const std::size_t n_hw = std::thread::hardware_concurrency();
            const std::size_t max_in_flight = n_hw > 0 ? n_hw * 2 : 1;
            run_bounded_async(
                pop_size, max_in_flight,
                [&checker, &pop, &original, &keep](std::size_t idx) {
                    return global_thread_pool().submit(
                        [&checker, &pop, &original, &keep, idx] {
                            if (tlsf_spec_implies(original, pop[idx], checker)
                                    .value_or(true)) {
                                keep[idx].store(1, std::memory_order_relaxed);
                            }
                        });
                },
                [](std::size_t) {});
            std::vector<tlsf::Specification> survivors;
            survivors.reserve(pop_size);
            for (std::size_t i = 0; i < pop_size; ++i) {
                if (keep[i].load(std::memory_order_relaxed) != 0U) {
                    survivors.push_back(pop[i]);
                }
            }
            return survivors;
        }};
}

FilterFunctionT<tlsf::Specification> tlsf_make_implication_filter(
    SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress) {
    return {"implication", [&checker, on_progress](
                               const std::vector<tlsf::Specification>& pop) {
                if (pop.size() <= 1) {
                    return pop;
                }
                const std::vector<uint8_t> subsumed =
                    compute_subsumed(pop, checker, on_progress);
                std::vector<tlsf::Specification> maximal;
                for (std::size_t i = 0; i < pop.size(); ++i) {
                    if (subsumed[i] == 0U) {
                        maximal.push_back(pop[i]);
                    }
                }
                return maximal;
            }};
}
