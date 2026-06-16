#include "filter/implication.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "requirement.hpp"
#include "thread_pool.hpp"

namespace {

// Returns true if `from` implies `to`, i.e. sat(from & !to) is UNSAT.
// Shortcuts on syntactically identical requirements (a requirement always
// implies itself) to avoid a black call, and to reuse this requirement pair's
// result across every spec pair that happens to share it.
bool requirement_implies(const Requirement& from, const Requirement& dest,
                         SatisfiabilityChecker& checker) {
    if (from == dest) {
        return true;
    }
    assert(from.m_ltl.has_value() && dest.m_ltl.has_value());
    const std::optional<bool> sat = checker.check_satisfiability(
        "(" + *from.m_ltl + ") & !(" + *dest.m_ltl + ")");
    return sat.has_value() && !sat.value();
}

// Returns true if every requirement in `to_reqs` is implied by some
// requirement in `from_reqs`. Sufficient (but not necessary) for the
// conjunction of from_reqs to imply the conjunction of to_reqs: a true
// implication that only holds via a combination of several from_reqs is
// missed, so this under-detects domination rather than ever over-detecting
// it.
bool all_implied_by_some(const std::vector<Requirement>& from_reqs,
                         const std::vector<Requirement>& to_reqs,
                         SatisfiabilityChecker& checker) {
    return std::all_of(
        to_reqs.begin(), to_reqs.end(), [&](const Requirement& dest) {
            return std::any_of(from_reqs.begin(), from_reqs.end(),
                               [&](const Requirement& from) {
                                   return requirement_implies(from, dest,
                                                              checker);
                               });
        });
}

// Sufficient condition for spec `from` to imply spec `to`, decomposed
// per-requirement following the standard assume-guarantee contract
// refinement rule: from's assumptions must each be implied by some
// assumption of `to` (to assumes no more than from requires), and to's
// guarantees must each be implied by some guarantee of `from`.
bool spec_implies(const Specification& from, const Specification& dest,
                  SatisfiabilityChecker& checker) {
    return all_implied_by_some(dest.m_assumptions, from.m_assumptions,
                               checker) &&
           all_implied_by_some(from.m_guarantees, dest.m_guarantees, checker);
}

// Checks one unordered pair of representative positions {a, b} in both
// directions and marks whichever side is strictly dominated, if any.
// Short-circuits if either endpoint is already subsumed (the "subsumption
// optimisation": once a spec is known redundant, no further comparison
// against it can change the outcome).
void check_pair(const std::vector<Specification>& pop,
                const std::vector<std::size_t>& representatives,
                std::vector<std::atomic<uint8_t>>& subsumed,
                SatisfiabilityChecker& checker, std::size_t a_pos,
                std::size_t b_pos) {
    if (subsumed[a_pos].load(std::memory_order_relaxed) != 0U ||
        subsumed[b_pos].load(std::memory_order_relaxed) != 0U) {
        ImplicationFilterStats::n_skipped.fetch_add(1,
                                                    std::memory_order_relaxed);
        return;
    }
    ImplicationFilterStats::n_comparisons.fetch_add(1,
                                                    std::memory_order_relaxed);
    const Specification& spec_a = pop[representatives[a_pos]];
    const Specification& spec_b = pop[representatives[b_pos]];
    const bool a_implies_b = spec_implies(spec_a, spec_b, checker);
    const bool b_implies_a = spec_implies(spec_b, spec_a, checker);
    if (a_implies_b && !b_implies_a) {
        subsumed[b_pos].store(1, std::memory_order_relaxed);
    } else if (b_implies_a && !a_implies_b) {
        subsumed[a_pos].store(1, std::memory_order_relaxed);
    }
}

// Computes which specs are subsumed: subsumed[j] = 1 iff some i strictly
// dominates j (i implies j but j does not imply i).
//
// Specs that are exact duplicates of an earlier spec relate identically to
// every other spec in the population (since spec_implies only depends on
// requirement structure), so only one representative per group of
// duplicates is run through the pairwise sweep; its result is copied to
// every member of the group afterwards.
std::vector<uint8_t> compute_subsumed(
    const std::vector<Specification>& pop, SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress) {
    const std::size_t pop_size = pop.size();

    std::unordered_map<Specification, std::size_t> rep_position_of;
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
    ImplicationFilterStats::n_duplicates.fetch_add(pop_size - n_reps,
                                                   std::memory_order_relaxed);

    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    pairs.reserve(n_reps * (n_reps - 1) / 2);
    for (std::size_t i = 0; i < n_reps; ++i) {
        for (std::size_t j = i + 1; j < n_reps; ++j) {
            pairs.emplace_back(i, j);
        }
    }
    std::vector<std::atomic<uint8_t>> subsumed_reps(n_reps);
    for (auto& flag : subsumed_reps) {
        flag.store(0, std::memory_order_relaxed);
    }
    const std::size_t n_hw = std::thread::hardware_concurrency();
    const std::size_t max_in_flight = n_hw > 0 ? n_hw * 2 : 1;
    std::size_t completed = 0;
    run_bounded_async(
        pairs.size(), max_in_flight,
        [&checker, &pop, &representatives, &subsumed_reps,
         &pairs](std::size_t idx) {
            const std::size_t a_pos = pairs[idx].first;
            const std::size_t b_pos = pairs[idx].second;
            return global_thread_pool().submit([&checker, &pop,
                                                &representatives,
                                                &subsumed_reps, a_pos, b_pos] {
                check_pair(pop, representatives, subsumed_reps, checker, a_pos,
                           b_pos);
            });
        },
        [&on_progress, &completed, total = pairs.size()](std::size_t) {
            if (on_progress) {
                on_progress(++completed, total);
            }
        });

    std::vector<uint8_t> result(pop_size, 0);
    for (std::size_t rep_pos = 0; rep_pos < n_reps; ++rep_pos) {
        const uint8_t status = subsumed_reps[rep_pos].load();
        for (const std::size_t idx : members[rep_pos]) {
            result[idx] = status;
        }
    }
    return result;
}

std::vector<Specification> keep_non_subsumed(
    const std::vector<Specification>& pop,
    const std::vector<uint8_t>& subsumed) {
    std::vector<Specification> maximal;
    for (std::size_t i = 0; i < pop.size(); ++i) {
        if (subsumed[i] == 0U) {
            maximal.push_back(pop[i]);
        }
    }
    return maximal;
}

}  // namespace

FilterFunction make_implication_filter(
    SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress) {
    return [&checker, on_progress](const std::vector<Specification>& pop) {
        ImplicationFilterStats::n_comparisons.store(0,
                                                    std::memory_order_relaxed);
        ImplicationFilterStats::n_skipped.store(0, std::memory_order_relaxed);
        ImplicationFilterStats::n_duplicates.store(0,
                                                   std::memory_order_relaxed);
        if (pop.size() <= 1) {
            return pop;
        }
        const std::vector<uint8_t> sub =
            compute_subsumed(pop, checker, on_progress);
        return keep_non_subsumed(pop, sub);
    };
}
