// Scratch: how much headroom a better MRS admission order has.
//
// Phase 1 (this file): for each TLSF example, the exact maximum realizable
// guarantee-part subset, against what the greedy walk keeps under each
// candidate order. Realizability is antitone in the guarantee set -- a
// superset of an unrealizable set is unrealizable -- so the solo and pairwise
// verdicts prune the descending-cardinality search.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bounded_async.hpp"
#include "fitness/status.hpp"
#include "runner/spot.hpp"
#include "thread_pool.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/guarantee_parts.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

std::size_t g_queries = 0;

struct Oracle {
    const tlsf::Specification* spec;
    const std::vector<tlsf::CoreFormula>* parts;
    RealizabilityChecker* real;

    bool operator()(const std::vector<std::size_t>& indices) const {
        ++g_queries;
        const tlsf::Specification sub =
            tlsf::build_part_subset(*spec, *parts, indices);
        return real->check_realizability_ltl(sub.to_ltl(), sub.m_inputs,
                                             sub.m_outputs)
                   .value_or(false) &&
               !tlsf_is_not_well_separated(sub, *real);
    }
};

std::vector<std::size_t> greedy(const Oracle& oracle,
                                const std::vector<std::size_t>& order) {
    std::vector<std::size_t> kept;
    for (const std::size_t part : order) {
        const auto slot = std::lower_bound(kept.begin(), kept.end(), part);
        const auto inserted = kept.insert(slot, part);
        if (!oracle(kept)) {
            kept.erase(inserted);
        }
    }
    return kept;
}

// Every k-subset of [0, n), passed to `visit`.
void each_subset(std::size_t n, std::size_t k,
                 const std::function<bool(const std::vector<std::size_t>&)>&
                     visit) {
    std::vector<std::size_t> pick(k);
    std::iota(pick.begin(), pick.end(), 0);
    if (k > n) {
        return;
    }
    while (true) {
        if (!visit(pick)) {
            return;
        }
        std::size_t i = k;
        while (i > 0 && pick[i - 1] == n - k + i - 1) {
            --i;
        }
        if (i == 0) {
            return;
        }
        ++pick[i - 1];
        for (std::size_t j = i; j < k; ++j) {
            pick[j] = pick[j - 1] + 1;
        }
    }
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    std::size_t parallel = 16;
    std::size_t level_cap = 40000;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--parallel=", 0) == 0) {
            parallel = std::stoul(arg.substr(11));
        } else if (arg.rfind("--level-cap=", 0) == 0) {
            level_cap = std::stoul(arg.substr(12));
        } else {
            paths.push_back(arg);
        }
    }
    set_thread_pool_size(parallel);
    RealizabilityChecker::set_max_concurrency(parallel);
    RealizabilityChecker::set_timeout(std::chrono::milliseconds(30000));
    RealizabilityChecker& real = global_real_checker();

    for (const std::string& path : paths) {
        const tlsf::Specification spec = tlsf::parse(read_file(path));
        const std::vector<tlsf::CoreFormula> parts =
            tlsf::split_guarantee_parts(spec);
        const std::size_t n = parts.size();
        const Oracle oracle{&spec, &parts, &real};
        g_queries = 0;
        const auto started = std::chrono::steady_clock::now();

        std::vector<std::size_t> index_order(n);
        std::iota(index_order.begin(), index_order.end(), 0);

        // Solo and pairwise verdicts: the degree order, and the prune table.
        std::vector<bool> solo(n, false);
        for (std::size_t i = 0; i < n; ++i) {
            solo[i] = oracle(std::vector<std::size_t>{i});
        }
        std::vector<std::vector<bool>> conflict(n, std::vector<bool>(n, false));
        std::vector<std::size_t> degree(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (oracle(std::vector<std::size_t>{i, j})) {
                    continue;
                }
                conflict[i][j] = conflict[j][i] = true;
                ++degree[i];
                ++degree[j];
            }
        }
        std::vector<std::size_t> degree_order = index_order;
        std::sort(degree_order.begin(), degree_order.end(),
                  [&](std::size_t a, std::size_t b) {
                      const std::size_t ra = solo[a] ? degree[a] : n + 1;
                      const std::size_t rb = solo[b] ? degree[b] : n + 1;
                      return std::make_pair(ra, a) < std::make_pair(rb, b);
                  });

        const std::vector<std::size_t> kept_index = greedy(oracle, index_order);
        const std::vector<std::size_t> kept_degree =
            greedy(oracle, degree_order);
        const std::size_t best_greedy =
            std::max(kept_index.size(), kept_degree.size());

        // Exact maximum, searched downward from n-1. A subset holding a
        // solo-unrealizable part or a conflicting pair is unrealizable by
        // antitonicity and costs no query.
        std::size_t optimum = best_greedy;
        std::vector<std::size_t> optimum_set =
            kept_degree.size() >= kept_index.size() ? kept_degree : kept_index;
        bool exact = true;
        for (std::size_t size = n; size > best_greedy; --size) {
            if (size == n) {
                continue;  // the whole spec is unrealizable by construction
            }
            std::vector<std::vector<std::size_t>> candidates;
            bool overflowed = false;
            each_subset(n, size,
                        [&](const std::vector<std::size_t>& subset) {
                            for (const std::size_t part : subset) {
                                if (!solo[part]) {
                                    return true;
                                }
                            }
                            for (std::size_t a = 0; a < subset.size(); ++a) {
                                for (std::size_t b = a + 1; b < subset.size();
                                     ++b) {
                                    if (conflict[subset[a]][subset[b]]) {
                                        return true;
                                    }
                                }
                            }
                            candidates.push_back(subset);
                            if (candidates.size() > level_cap) {
                                overflowed = true;
                                return false;
                            }
                            return true;
                        });
            if (overflowed) {
                exact = false;
                break;
            }
            bool found = false;
            for (const std::vector<std::size_t>& subset : candidates) {
                if (oracle(subset)) {
                    optimum = size;
                    optimum_set = subset;
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }

        // An order that admits a maximum subset first, so the walk attains the
        // optimum on the input specification by construction.
        std::vector<std::size_t> mrs_order = optimum_set;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::find(optimum_set.begin(), optimum_set.end(), i) ==
                optimum_set.end()) {
                mrs_order.push_back(i);
            }
        }
        const std::vector<std::size_t> kept_mrs = greedy(oracle, mrs_order);

        nlohmann::json out;
        out["family"] =
            std::filesystem::path(path).parent_path().filename().string();
        out["n_parts"] = n;
        out["kept_index"] = kept_index.size();
        out["kept_degree"] = kept_degree.size();
        out["kept_mrs_order"] = kept_mrs.size();
        out["optimum"] = optimum;
        out["optimum_exact"] = exact;
        out["optimum_set"] = optimum_set;
        out["mrs_order"] = mrs_order;
        out["degree_order"] = degree_order;
        out["queries"] = g_queries;
        out["wall_s"] = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - started)
                            .count();
        std::cout << out.dump() << std::endl;
    }
    return 0;
}
