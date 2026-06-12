#include "filter/implication.hpp"

#include <algorithm>
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

// Returns a flat matrix where entry [i * pop_size + j] == 1 iff pop[i]
// logically implies pop[j]. All pairs are checked in parallel.
std::vector<uint8_t> compute_implies_matrix(
    const std::vector<std::string>& ltls, std::size_t pop_size,
    SatisfiabilityChecker& checker) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    pairs.reserve(pop_size * (pop_size - 1));
    for (std::size_t idx_i = 0; idx_i < pop_size; ++idx_i) {
        for (std::size_t idx_j = 0; idx_j < pop_size; ++idx_j) {
            if (idx_i != idx_j) {
                pairs.emplace_back(idx_i, idx_j);
            }
        }
    }

    // Each cell is written by exactly one async task; uint8_t avoids the
    // bit-packing aliasing hazard of vector<bool>.
    std::vector<uint8_t> mat(pop_size * pop_size, 0);

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
                [&checker, &ltls, &mat, pop_size, from_idx, to_idx] {
                    const bool sat = checker.check_satisfiability(
                        "(" + ltls[from_idx] + ") & !(" + ltls[to_idx] + ")");
                    if (!sat) {
                        mat[from_idx * pop_size + to_idx] = 1;
                    }
                }));
        }
        for (auto& fut : futures) {
            fut.get();
        }
    }
    return mat;
}

// Returns only specs not strictly dominated by another. Spec j is dominated
// iff some i exists with implies[i][j] AND NOT implies[j][i].
std::vector<Specification> keep_maximal(const std::vector<Specification>& pop,
                                        const std::vector<uint8_t>& mat,
                                        std::size_t pop_size) {
    std::vector<Specification> maximal;
    for (std::size_t idx_j = 0; idx_j < pop_size; ++idx_j) {
        bool dominated = false;
        for (std::size_t idx_i = 0; idx_i < pop_size && !dominated; ++idx_i) {
            if (idx_i == idx_j) {
                continue;
            }
            if (mat[idx_i * pop_size + idx_j] != 0 &&
                mat[idx_j * pop_size + idx_i] == 0) {
                dominated = true;
            }
        }
        if (!dominated) {
            maximal.push_back(pop[idx_j]);
        }
    }
    return maximal;
}

}  // namespace

FilterFunction make_implication_filter(SatisfiabilityChecker& checker) {
    return [&checker](const std::vector<Specification>& pop) {
        const std::size_t pop_size = pop.size();
        if (pop_size <= 1) {
            return pop;
        }
        std::vector<std::string> ltls;
        ltls.reserve(pop_size);
        for (const Specification& spec : pop) {
            ltls.push_back(build_spec_ltl(spec));
        }
        const std::vector<uint8_t> mat =
            compute_implies_matrix(ltls, pop_size, checker);
        return keep_maximal(pop, mat, pop_size);
    };
}
