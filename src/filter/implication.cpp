#include "filter/implication.hpp"

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "filter/implication_check.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "requirement.hpp"
#include "thread_pool.hpp"

namespace {

// Orders the two members of an equivalence class so exactly one survives.
// Higher syntactic similarity to the original wins; `operator<` settles the
// rest. Similarity alone is not a total order -- two distinct specs routinely
// score identically against the original -- and a rule that is not total would
// leave the survivor decided by whichever pair the concurrent sweep happens to
// finish first, which is how the same seed would stop reproducing.
bool prefer_a(const Specification& spec_a, const Specification& spec_b,
              double key_a, double key_b) {
    if (key_a != key_b) {
        return key_a > key_b;
    }
    return spec_b < spec_a;
}

// Checks one unordered pair of representative positions {a, b} in both
// directions and marks the dominated side, if any.
//
// Strict domination (one direction only) subsumes the weaker side. Mutual
// implication subsumes whichever side `prefer_a` ranks lower, so an
// equivalence class contributes one member rather than all of them.
//
// Short-circuits if either endpoint is already subsumed (the "subsumption
// optimisation": once a spec is known redundant, no further comparison
// against it can change the outcome). That optimisation needs the dominance
// relation to be *transitive*, or a skipped pair could strand a dominated
// spec unmarked. It stays transitive with the tie-break in: within a class
// the order is `prefer_a`, which is total, and across classes equivalent
// specs imply exactly the same things, so a strict edge into or out of one
// member is a strict edge for every member.
void check_pair(const std::vector<Specification>& pop,
                const std::vector<std::size_t>& representatives,
                const std::vector<double>& keys,
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
    // A timed-out check cannot establish dominance, so an uncertain pair keeps
    // both endpoints. An equivalence that only one direction proves in time
    // therefore reads as strict domination, and one that neither proves keeps
    // both -- the collapse is best-effort, like the rest of the sweep.
    const bool a_implies_b =
        spec_implies(spec_a, spec_b, checker).value_or(false);
    const bool b_implies_a =
        spec_implies(spec_b, spec_a, checker).value_or(false);
    if (a_implies_b && b_implies_a) {
        const bool keep_a = prefer_a(spec_a, spec_b, keys[a_pos], keys[b_pos]);
        subsumed[keep_a ? b_pos : a_pos].store(1, std::memory_order_relaxed);
        ImplicationFilterStats::n_equivalent_collapsed.fetch_add(
            1, std::memory_order_relaxed);
    } else if (a_implies_b) {
        subsumed[b_pos].store(1, std::memory_order_relaxed);
    } else if (b_implies_a) {
        subsumed[a_pos].store(1, std::memory_order_relaxed);
    }
}

// Computes which specs are subsumed: subsumed[j] = 1 iff some i dominates j,
// meaning i strictly implies j, or i is equivalent to j and outranks it under
// `prefer_a`.
//
// Specs that are exact duplicates of an earlier spec relate identically to
// every other spec in the population (since spec_implies only depends on
// requirement structure), so only one representative per group of
// duplicates is run through the pairwise sweep. A duplicate group is an
// equivalence class whose members need no solver call to recognise, so only
// the representative survives it; the rest of the group is subsumed outright.
std::vector<uint8_t> compute_subsumed(
    const std::vector<Specification>& pop, SatisfiabilityChecker& checker,
    const SimilarityKey& similarity,
    const GenerationProgressCallback& on_progress) {
    const std::size_t pop_size = pop.size();

    std::unordered_map<Specification, std::size_t> rep_position_of;
    std::vector<std::size_t> representatives;
    for (std::size_t i = 0; i < pop_size; ++i) {
        if (rep_position_of.try_emplace(pop[i], representatives.size())
                .second) {
            representatives.push_back(i);
        }
    }
    const std::size_t n_reps = representatives.size();
    ImplicationFilterStats::n_duplicates.fetch_add(pop_size - n_reps,
                                                   std::memory_order_relaxed);

    // One similarity call per representative rather than per pair: the key is
    // a property of the spec, and the sweep is quadratic in n_reps.
    std::vector<double> keys(n_reps, 0.0);
    if (similarity) {
        for (std::size_t rep_pos = 0; rep_pos < n_reps; ++rep_pos) {
            keys[rep_pos] = similarity(pop[representatives[rep_pos]]);
        }
    }

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
    const std::size_t max_in_flight = dispatch_window();
    std::size_t completed = 0;
    run_bounded_async(
        pairs.size(), max_in_flight,
        [&checker, &pop, &representatives, &keys, &subsumed_reps,
         &pairs](std::size_t idx) {
            const std::size_t a_pos = pairs[idx].first;
            const std::size_t b_pos = pairs[idx].second;
            return [&checker, &pop, &representatives, &keys, &subsumed_reps,
                    a_pos, b_pos] {
                check_pair(pop, representatives, keys, subsumed_reps, checker,
                           a_pos, b_pos);
            };
        },
        [&on_progress, &completed, total = pairs.size()](std::size_t) {
            if (on_progress) {
                on_progress(++completed, total);
            }
        });

    std::vector<uint8_t> result(pop_size, 1);
    for (std::size_t rep_pos = 0; rep_pos < n_reps; ++rep_pos) {
        // Only the group's representative can survive: every other member is
        // structurally equal to it, so keeping one is what the equivalence
        // rule above would decide anyway, with no solver call needed.
        result[representatives[rep_pos]] = subsumed_reps[rep_pos].load();
    }
    return result;
}

std::vector<Specification> keep_non_subsumed(
    std::vector<Specification> pop, const std::vector<uint8_t>& subsumed) {
    std::vector<Specification> maximal;
    for (std::size_t i = 0; i < pop.size(); ++i) {
        if (subsumed[i] == 0U) {
            maximal.push_back(std::move(pop[i]));
        }
    }
    return maximal;
}

}  // namespace

FilterFunction make_dedup_filter() {
    return {"dedup",
            [](std::vector<Specification> pop) {
                std::unordered_set<Specification> seen;
                seen.reserve(pop.size());
                std::vector<Specification> survivors;
                survivors.reserve(pop.size());
                for (Specification& spec : pop) {
                    // The set has to own a copy to key on; the survivor is
                    // then moved, so a kept candidate costs one copy rather
                    // than two.
                    if (seen.insert(spec).second) {
                        survivors.push_back(std::move(spec));
                    }
                }
                return survivors;
            },
            FilterKind::Preference};
}

FilterFunction make_weakening_filter(Specification original,
                                     SatisfiabilityChecker& checker) {
    return {"weakening", [original = std::move(original),
                          &checker](std::vector<Specification> pop) {
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
                            // A timed-out check retains the candidate: dropping
                            // on an unknown answer would make survival depend
                            // on machine load.
                            if (spec_implies(original, pop[idx], checker)
                                    .value_or(true)) {
                                keep[idx].store(1, std::memory_order_relaxed);
                            }
                        };
                    },
                    [](std::size_t) {});
                std::vector<Specification> survivors;
                survivors.reserve(pop_size);
                for (std::size_t i = 0; i < pop_size; ++i) {
                    if (keep[i].load(std::memory_order_relaxed) != 0U) {
                        survivors.push_back(std::move(pop[i]));
                    }
                }
                return survivors;
            }};
}

FilterFunction make_implication_filter(
    SatisfiabilityChecker& checker, SimilarityKey similarity,
    const GenerationProgressCallback& on_progress) {
    return {"implication",
            [&checker, similarity = std::move(similarity),
             on_progress](std::vector<Specification> pop) {
                ImplicationFilterStats::n_comparisons.store(
                    0, std::memory_order_relaxed);
                ImplicationFilterStats::n_skipped.store(
                    0, std::memory_order_relaxed);
                ImplicationFilterStats::n_duplicates.store(
                    0, std::memory_order_relaxed);
                ImplicationFilterStats::n_timeouts.store(
                    0, std::memory_order_relaxed);
                ImplicationFilterStats::n_equivalent_collapsed.store(
                    0, std::memory_order_relaxed);
                if (pop.size() <= 1) {
                    return pop;
                }
                const std::vector<uint8_t> sub =
                    compute_subsumed(pop, checker, similarity, on_progress);
                return keep_non_subsumed(std::move(pop), sub);
            },
            FilterKind::Preference};
}

SimilarityKey syntactic_similarity_key(Specification original,
                                       const Config& cfg) {
    // Both captured by value: the returned key outlives this call, and the
    // filters it goes into are held for the whole run.
    return [original = std::move(original),
            cfg](const Specification& spec) mutable {
        return syntactic_similarity(spec, original, cfg);
    };
}
