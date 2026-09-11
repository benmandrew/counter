#include "filter/implication.hpp"

#include <atomic>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "filter/antichain.hpp"
#include "filter/implication_check.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "requirement.hpp"
#include "thread_pool.hpp"

namespace {

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
    return {
        "implication",
        [&checker, similarity = std::move(similarity),
         on_progress](std::vector<Specification> pop) {
            antichain::reset_stats();
            if (pop.size() <= 1) {
                return pop;
            }
            const std::vector<uint8_t> sub = antichain::subsumed_in(
                pop,
                [&checker](const Specification& lhs, const Specification& rhs) {
                    return spec_implies(lhs, rhs, checker).value_or(false);
                },
                similarity, on_progress);
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
