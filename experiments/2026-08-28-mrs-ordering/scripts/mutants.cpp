// Scratch, phase 2: does a better admission order survive replay onto mutants?
//
// The order is computed once on the input specification and replayed on every
// candidate, so the input-specification headroom phase 1 measures is only worth
// having if it carries. Scores go through tlsf_status, the shipping path.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "thread_pool.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/mutation.hpp"
#include "tlsf/guarantee_parts.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    std::size_t parallel = 16;
    std::size_t chains = 8;
    std::size_t depth = 6;
    std::size_t restarts = 4;
    std::size_t seed = 1;
    bool per_candidate = false;
    bool degree_only = false;
    std::size_t gate = 0;  // 0 = ungated; k = extra orders only when the
                           // degree walk keeps at most n_parts - k
    std::string orders_path;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--parallel=", 0) == 0) {
            parallel = std::stoul(arg.substr(11));
        } else if (arg.rfind("--chains=", 0) == 0) {
            chains = std::stoul(arg.substr(9));
        } else if (arg.rfind("--depth=", 0) == 0) {
            depth = std::stoul(arg.substr(8));
        } else if (arg.rfind("--restarts=", 0) == 0) {
            restarts = std::stoul(arg.substr(11));
        } else if (arg.rfind("--seed=", 0) == 0) {
            seed = std::stoul(arg.substr(7));
        } else if (arg.rfind("--gate=", 0) == 0) {
            gate = std::stoul(arg.substr(7));
        } else if (arg == "--degree-only") {
            degree_only = true;
        } else if (arg == "--per-candidate") {
            per_candidate = true;
        } else if (arg.rfind("--orders=", 0) == 0) {
            orders_path = arg.substr(9);
        } else {
            paths.push_back(arg);
        }
    }
    set_thread_pool_size(parallel);
    RealizabilityChecker::set_max_concurrency(parallel);
    RealizabilityChecker::set_timeout(std::chrono::milliseconds(30000));

    nlohmann::json orders_by_family;
    {
        std::ifstream in(orders_path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            const nlohmann::json row = nlohmann::json::parse(line);
            orders_by_family[row["family"].get<std::string>()] = row;
        }
    }

    Config cfg;  // shipping defaults
    for (const std::string& path : paths) {
        const std::string family =
            std::filesystem::path(path).parent_path().filename().string();
        const tlsf::Specification original = tlsf::parse(read_file(path));
        if (!orders_by_family.contains(family)) {
            std::cerr << family << ": no order row\n";
            continue;
        }
        const nlohmann::json& row = orders_by_family[family];
        const std::vector<std::size_t> degree_order =
            row["degree_order"].get<std::vector<std::size_t>>();
        const std::vector<std::size_t> mrs_order =
            row["mrs_order"].get<std::vector<std::size_t>>();
        const std::size_t n = row["n_parts"].get<std::size_t>();

        std::mt19937_64 shuffler(seed * 7919 + 13);
        std::vector<std::vector<std::size_t>> random_orders;
        for (std::size_t r = 0; r < restarts; ++r) {
            std::vector<std::size_t> order(n);
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), shuffler);
            random_orders.push_back(std::move(order));
        }

        const auto started = std::chrono::steady_clock::now();
        const std::size_t hits_before = RealizabilityChecker::n_cache_hits;
        const std::size_t misses_before = RealizabilityChecker::n_cache_misses;
        const double real_time_before = RealizabilityChecker::total_time_s;
        const double sat_time_before = SatisfiabilityChecker::total_time_s;
        const std::size_t sat_misses_before =
            SatisfiabilityChecker::n_cache_misses.load();
        nlohmann::json samples = nlohmann::json::array();
        for (std::size_t chain = 0; chain < chains; ++chain) {
            const RandomSource rng =
                make_random_source_from_seed(seed * 1000 + chain);
            tlsf::Specification spec = original;
            for (std::size_t step = 0; step < depth; ++step) {
                spec = tlsf_mutate(spec, rng, cfg);
                nlohmann::json s;
                s["chain"] = chain;
                s["depth"] = step + 1;
                const double degree_score =
                    tlsf_status(spec, cfg, degree_order);
                s["degree"] = degree_score;
                // The gate: a candidate the degree walk already brings within
                // k-1 parts of its own ceiling gets no second opinion.
                bool open_gate = true;
                if (gate > 0) {
                    const std::size_t mut_parts =
                        tlsf::split_guarantee_parts(spec).size();
                    const double threshold =
                        mut_parts > gate
                            ? static_cast<double>(mut_parts - gate) /
                                  static_cast<double>(mut_parts)
                            : 0.0;
                    open_gate = degree_score <= threshold + 1e-9;
                }
                s["gate_open"] = open_gate;
                if (!degree_only && open_gate) {
                    s["index"] = tlsf_status(spec, cfg, {});
                    s["mrs"] = tlsf_status(spec, cfg, mrs_order);
                }
                if (per_candidate) {
                    for (std::vector<std::size_t>& order : random_orders) {
                        std::shuffle(order.begin(), order.end(), shuffler);
                    }
                }
                double best = 0.0;
                for (const std::vector<std::size_t>& order :
                     (open_gate ? random_orders
                                : std::vector<std::vector<std::size_t>>{})) {
                    best = std::max(best, tlsf_status(spec, cfg, order));
                }
                if (!random_orders.empty()) {
                    s["random_best"] = best;
                }
                samples.push_back(std::move(s));
            }
        }
        nlohmann::json out;
        out["family"] = family;
        out["n_parts"] = n;
        out["samples"] = std::move(samples);
        out["cache_hits"] = RealizabilityChecker::n_cache_hits - hits_before;
        out["cache_misses"] =
            RealizabilityChecker::n_cache_misses - misses_before;
        out["real_tool_s"] = RealizabilityChecker::total_time_s - real_time_before;
        out["sat_tool_s"] = SatisfiabilityChecker::total_time_s - sat_time_before;
        out["sat_misses"] =
            SatisfiabilityChecker::n_cache_misses.load() - sat_misses_before;
        out["per_candidate"] = per_candidate;
        out["restarts"] = restarts;
        out["wall_s"] = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - started)
                            .count();
        std::cout << out.dump() << std::endl;
    }
    return 0;
}
