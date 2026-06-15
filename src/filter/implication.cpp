#include "filter/implication.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <future>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "requirement.hpp"

namespace {

std::string build_spec_ltl(const Specification& spec) {
    auto conjoin = [](const std::vector<Requirement>& reqs) {
        std::string result;
        bool first = true;
        for (const Requirement& req : reqs) {
            assert(req.m_ltl.has_value());
            if (!first) {
                result += " & ";
            }
            result += "(" + *req.m_ltl + ")";
            first = false;
        }
        return result;
    };
    if (spec.m_assumptions.empty()) {
        return conjoin(spec.m_guarantees);
    }
    return "(" + conjoin(spec.m_assumptions) + ") -> (" +
           conjoin(spec.m_guarantees) + ")";
}

// Checks one ordered pair: if from strictly dominates to, marks subsumed[to].
// Short-circuits if either endpoint is already subsumed.
// Checks the reverse direction when forward implication holds to preserve
// mutual equivalences (A <=> B keeps both).
void check_pair(const std::vector<std::string>& ltls,
                std::vector<std::atomic<uint8_t>>& subsumed,
                SatisfiabilityChecker& checker, std::size_t from_idx,
                std::size_t to_idx) {
    if (subsumed[from_idx].load(std::memory_order_relaxed) != 0U ||
        subsumed[to_idx].load(std::memory_order_relaxed) != 0U) {
        return;
    }
    const bool sat = checker.check_satisfiability(
        "(" + ltls[from_idx] + ") & !(" + ltls[to_idx] + ")");
    if (!sat) {
        const bool reverse_sat = checker.check_satisfiability(
            "(" + ltls[to_idx] + ") & !(" + ltls[from_idx] + ")");
        if (reverse_sat) {
            subsumed[to_idx].store(1, std::memory_order_relaxed);
        }
    }
}

// Computes which specs are subsumed: subsumed[j] = 1 iff some i strictly
// dominates j (i implies j but j does not imply i).
std::vector<uint8_t> compute_subsumed(
    const std::vector<std::string>& ltls, std::size_t pop_size,
    SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    pairs.reserve(pop_size * (pop_size - 1));
    for (std::size_t i = 0; i < pop_size; ++i) {
        for (std::size_t j = 0; j < pop_size; ++j) {
            if (i != j) {
                pairs.emplace_back(i, j);
            }
        }
    }
    std::vector<std::atomic<uint8_t>> subsumed(pop_size);
    for (auto& flag : subsumed) {
        flag.store(0, std::memory_order_relaxed);
    }
    const std::size_t n_hw = std::thread::hardware_concurrency();
    const std::size_t batch_size = n_hw > 0 ? n_hw * 2 : 1;
    for (std::size_t batch_start = 0; batch_start < pairs.size();
         batch_start += batch_size) {
        const std::size_t batch_end =
            std::min(batch_start + batch_size, pairs.size());
        std::vector<std::future<void>> futures;
        futures.reserve(batch_end - batch_start);
        for (std::size_t k = batch_start; k < batch_end; ++k) {
            const std::size_t from_idx = pairs[k].first;
            const std::size_t to_idx = pairs[k].second;
            futures.push_back(std::async(
                std::launch::async,
                [&checker, &ltls, &subsumed, from_idx, to_idx] {
                    check_pair(ltls, subsumed, checker, from_idx, to_idx);
                }));
        }
        for (auto& fut : futures) {
            fut.get();
        }
        if (on_progress) {
            on_progress(batch_end, pairs.size());
        }
    }
    std::vector<uint8_t> result(pop_size);
    for (std::size_t i = 0; i < pop_size; ++i) {
        result[i] = subsumed[i].load();
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
        const std::size_t pop_size = pop.size();
        if (pop_size <= 1) {
            return pop;
        }
        std::vector<std::string> ltls;
        ltls.reserve(pop_size);
        for (const Specification& spec : pop) {
            ltls.push_back(build_spec_ltl(spec));
        }
        const std::vector<uint8_t> sub =
            compute_subsumed(ltls, pop_size, checker, on_progress);
        return keep_non_subsumed(pop, sub);
    };
}
