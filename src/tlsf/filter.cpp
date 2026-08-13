#include "tlsf/filter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "prop_formula.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "thread_pool.hpp"
#include "tlsf/specification.hpp"

namespace {

// Collects the atom names appearing in a (possibly temporal) formula.
void collect_atoms(const Formula& formula,
                   std::unordered_set<std::string>& out) {
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            if (const std::optional<std::string> name = formula.atom_name()) {
                out.insert(*name);
            }
            return;
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally:
            if (const std::optional<Formula> child = formula.unary_child()) {
                collect_atoms(*child, out);
            }
            return;
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff:
        case Formula::Kind::Until:
        case Formula::Kind::Release:
        case Formula::Kind::WeakUntil:
            if (const std::optional<std::pair<Formula, Formula>> children =
                    formula.binary_children()) {
                collect_atoms(children->first, out);
                collect_atoms(children->second, out);
            }
            return;
    }
}

// True if any assumption-side formula (INITIALLY, REQUIRE, ASSUME) references
// an output atom. Only then can the system force the assumptions to fail, so
// only then is the well-separation ltlsynt query worth running; an assumption
// over inputs alone is well-separated by construction.
bool assumptions_reference_output(const tlsf::Specification& spec) {
    const std::unordered_set<std::string> outputs(spec.m_outputs.begin(),
                                                  spec.m_outputs.end());
    if (outputs.empty()) {
        return false;
    }
    for (const tlsf::Section* section : tlsf::assumption_sections_of(spec)) {
        for (const tlsf::SectionEntry& entry : *section) {
            if (entry.m_removed) {
                continue;
            }
            std::unordered_set<std::string> atoms;
            collect_atoms(entry.m_formula, atoms);
            for (const std::string& atom : atoms) {
                if (outputs.count(atom) != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Deleted conjuncts are excluded from both scans: the cap is on the size of the
// specification, and a deleted conjunct is not in it. Counting one would keep
// charging a candidate for a formula it has already dropped.
std::size_t max_formula_size(const tlsf::Specification& spec) {
    std::size_t max = 0;
    for (const tlsf::Section* section : tlsf::sections_of(spec)) {
        for (const tlsf::SectionEntry& entry : *section) {
            if (entry.m_removed) {
                continue;
            }
            max = std::max(max, entry.m_formula.n_subformulae());
        }
    }
    return max;
}

bool any_formula_exceeds(const tlsf::Specification& spec, std::size_t cap) {
    for (const tlsf::Section* section : tlsf::sections_of(spec)) {
        for (const tlsf::SectionEntry& entry : *section) {
            if (!entry.m_removed && entry.m_formula.n_subformulae() > cap) {
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
    const std::size_t max_in_flight = dispatch_window();
    std::size_t completed = 0;
    run_bounded_async(
        pairs.size(), max_in_flight,
        [&checker, &pop, &representatives, &subsumed, &pairs](std::size_t idx) {
            const std::size_t pos_a = pairs[idx].first;
            const std::size_t pos_b = pairs[idx].second;
            return [&checker, &pop, &representatives, &subsumed, pos_a, pos_b] {
                check_pair(pop, representatives, subsumed, checker, pos_a,
                           pos_b);
            };
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

// FRETISH routes every element-wise filter through make_predicate_filter, so
// parallelising that one function covered all of them. TLSF filters each
// hand-roll their loop, so the same index-collect pattern lives here for the
// two whose predicate is an external solver call. Verdicts are collected by
// index and the survivors rebuilt in population order, so the result matches a
// serial sweep exactly.
std::vector<tlsf::Specification> filter_in_parallel(
    const std::vector<tlsf::Specification>& pop, std::size_t max_in_flight,
    const std::function<bool(const tlsf::Specification&)>& predicate) {
    std::vector<char> keep(pop.size(), 0);
    if (max_in_flight <= 1) {
        for (std::size_t idx = 0; idx < pop.size(); ++idx) {
            keep[idx] = predicate(pop[idx]) ? 1 : 0;
        }
    } else {
        run_bounded_async(
            pop.size(), max_in_flight,
            [&predicate, &pop](std::size_t idx) {
                return
                    [&predicate, &spec = pop[idx]] { return predicate(spec); };
            },
            [&keep](std::size_t idx, bool verdict) {
                keep[idx] = verdict ? 1 : 0;
            });
    }
    std::vector<tlsf::Specification> survivors;
    survivors.reserve(pop.size());
    for (std::size_t idx = 0; idx < pop.size(); ++idx) {
        if (keep[idx] != 0) {
            survivors.push_back(pop[idx]);
        }
    }
    return survivors;
}

}  // namespace

FilterFunctionT<tlsf::Specification> tlsf_make_dedup_filter() {
    return {"dedup",
            [](const std::vector<tlsf::Specification>& pop) {
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
            },
            FilterKind::Preference};
}

bool tlsf_is_trivially_vacuous(const tlsf::Specification& spec) {
    // A deleted conjunct is exempt: its residual content is not part of the
    // specification, so it must not make one read as vacuous.
    auto any_atom = [](const tlsf::Section& section, const char* atom) {
        return std::any_of(section.begin(), section.end(),
                           [atom](const tlsf::SectionEntry& entry) {
                               return !entry.m_removed &&
                                      entry.m_formula.atom_name() == atom;
                           });
    };
    return any_atom(spec.m_initially, "false") ||
           any_atom(spec.m_require, "false") ||
           any_atom(spec.m_assume, "false") ||
           any_atom(spec.m_preset, "true") || any_atom(spec.m_assert, "true") ||
           any_atom(spec.m_guarantee, "true");
}

bool tlsf_has_unsatisfiable_assumptions(const tlsf::Specification& spec,
                                        SatisfiabilityChecker& checker) {
    const bool no_assumptions = tlsf::count_live(spec.m_initially) == 0 &&
                                tlsf::count_live(spec.m_require) == 0 &&
                                tlsf::count_live(spec.m_assume) == 0;
    if (no_assumptions) {
        return false;
    }
    // Timeout: treat as satisfiable. Dropping on an unknown answer would make
    // the verdict depend on machine load.
    return !checker.check_satisfiability(spec.assumption_ltl()).value_or(true);
}

bool tlsf_has_valid_guarantee(const tlsf::Specification& spec,
                              SatisfiabilityChecker& checker) {
    auto any_valid = [&checker](const tlsf::Section& section) {
        for (const tlsf::SectionEntry& entry : section) {
            // A deleted conjunct is not a guarantee: it must not be able to
            // make the specification read as vacuously satisfied.
            if (entry.m_removed) {
                continue;
            }
            // Keyed on the negated formula alone, so the cache hits across
            // candidates and generations rather than once per guarantee side.
            const std::optional<bool> falsifiable =
                checker.check_satisfiability("!(" +
                                             entry.m_formula.to_string() + ")");
            // Timeout: treat as falsifiable, as the assumption check treats an
            // unknown answer as satisfiable. A non-answer never drops a
            // candidate.
            if (!falsifiable.value_or(true)) {
                return true;
            }
        }
        return false;
    };
    // ASSERT is G-wrapped by the lowering, but `G psi` is valid exactly when
    // psi is, so the raw formula is the query either way -- and the smaller
    // one.
    return any_valid(spec.m_preset) || any_valid(spec.m_assert) ||
           any_valid(spec.m_guarantee);
}

bool tlsf_is_vacuous(const tlsf::Specification& spec,
                     SatisfiabilityChecker& checker) {
    return tlsf_is_trivially_vacuous(spec) ||
           tlsf_has_valid_guarantee(spec, checker) ||
           tlsf_has_unsatisfiable_assumptions(spec, checker);
}

FilterFunctionT<tlsf::Specification> tlsf_make_vacuity_filter(
    std::size_t max_in_flight) {
    return {"vacuity",
            [max_in_flight](const std::vector<tlsf::Specification>& pop) {
                return filter_in_parallel(
                    pop, max_in_flight, [](const tlsf::Specification& spec) {
                        return !tlsf_is_vacuous(spec, global_sat_checker());
                    });
            }};
}

bool tlsf_is_not_well_separated(const tlsf::Specification& spec,
                                RealizabilityChecker& checker) {
    if (!assumptions_reference_output(spec)) {
        return false;
    }
    // Not well-separated exactly when (assumptions) -> false is realizable: the
    // system has a strategy forcing its own assumptions to fail. An undecided
    // query reads as realizable and so drops the candidate, for the reason
    // given in filter/well_separation.cpp.
    const std::string formula = "(" + spec.assumption_ltl() + ") -> (false)";
    return checker
        .check_realizability_ltl(formula, spec.m_inputs, spec.m_outputs)
        .value_or(true);
}

FilterFunctionT<tlsf::Specification> tlsf_make_well_separation_filter(
    RealizabilityChecker& checker, std::size_t max_in_flight) {
    return {
        "not-well-separated",
        [&checker, max_in_flight](const std::vector<tlsf::Specification>& pop) {
            return filter_in_parallel(
                pop, max_in_flight,
                [&checker](const tlsf::Specification& candidate) {
                    return !tlsf_is_not_well_separated(candidate, checker);
                });
        }};
}

FilterFunctionT<tlsf::Specification> tlsf_make_predicate_filter(
    std::string name, std::function<bool(const tlsf::Specification&)> predicate,
    std::size_t max_in_flight, FilterKind kind) {
    return {std::move(name),
            [predicate = std::move(predicate),
             max_in_flight](const std::vector<tlsf::Specification>& pop) {
                return filter_in_parallel(pop, max_in_flight, predicate);
            },
            kind};
}

std::vector<CorrectnessCheckT<tlsf::Specification>> tlsf_correctness_checks(
    SatisfiabilityChecker& sat, RealizabilityChecker& real) {
    std::vector<CorrectnessCheckT<tlsf::Specification>> checks;
    checks.push_back({"vacuity",
                      [&sat](const tlsf::Specification& spec) {
                          return !tlsf_is_vacuous(spec, sat);
                      },
                      &Config::run_vacuity_filter});
    checks.push_back({"not-well-separated",
                      [&real](const tlsf::Specification& spec) {
                          return !tlsf_is_not_well_separated(spec, real);
                      },
                      &Config::run_well_separation_filter});
    return checks;
}

std::optional<bool> tlsf_spec_implies(const tlsf::Specification& from,
                                      const tlsf::Specification& dest,
                                      SatisfiabilityChecker& checker) {
    if (from == dest) {
        return true;
    }
    const std::optional<bool> sat = checker.check_satisfiability(
        "(" + from.to_ltl() + ") & !(" + dest.to_ltl() + ")",
        QueryPolarity::ExpectUnsat);
    if (!sat.has_value()) {
        return std::nullopt;
    }
    return !sat.value();
}

FilterFunctionT<tlsf::Specification> tlsf_make_bloat_cap_filter(
    const tlsf::Specification& original, double max_ratio) {
    const std::size_t original_max = max_formula_size(original);
    return {
        "bloat-cap",
        [original_max, max_ratio](const std::vector<tlsf::Specification>& pop) {
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
        },
        FilterKind::Preference};
}

FilterFunctionT<tlsf::Specification> tlsf_make_weakening_filter(
    tlsf::Specification original, SatisfiabilityChecker& checker) {
    return {"weakening", [original = std::move(original), &checker](
                             const std::vector<tlsf::Specification>& pop) {
                const std::size_t pop_size = pop.size();
                std::vector<std::atomic<uint8_t>> keep(pop_size);
                for (auto& flag : keep) {
                    flag.store(0, std::memory_order_relaxed);
                }
                const std::size_t max_in_flight = dispatch_window();
                run_bounded_async(
                    pop_size, max_in_flight,
                    [&checker, &pop, &original, &keep](std::size_t idx) {
                        return [&checker, &pop, &original, &keep, idx] {
                            if (tlsf_spec_implies(original, pop[idx], checker)
                                    .value_or(true)) {
                                keep[idx].store(1, std::memory_order_relaxed);
                            }
                        };
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
    return {
        "implication",
        [&checker, on_progress](const std::vector<tlsf::Specification>& pop) {
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
        },
        FilterKind::Preference};
}
