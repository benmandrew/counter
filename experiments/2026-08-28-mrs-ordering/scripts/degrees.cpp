// Scratch diagnostic: dumps the pairwise conflict matrix behind
// conflict_degree_order for each TLSF specification given.
//
// The oracle here is copied from tlsf_mrs_admission_order in
// src/tlsf/fitness.cpp, so the degrees printed are the ones the shipping
// MrsAdmissionOrder::Degree walk sorts on -- with the undecided verdicts kept
// distinct, which that oracle folds into "unrealizable" via value_or(false).

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "bounded_async.hpp"
#include "runner/spot.hpp"
#include "thread_pool.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/guarantee_parts.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

struct Verdict {
    std::optional<bool> realizable;  // raw ltlsynt verdict
    bool well_separated = false;
    bool kept() const { return realizable.value_or(false) && well_separated; }
};

Verdict ask(const tlsf::Specification& spec, RealizabilityChecker& real) {
    Verdict v;
    v.realizable = real.check_realizability_ltl(spec.to_ltl(), spec.m_inputs,
                                                spec.m_outputs);
    v.well_separated = !tlsf_is_not_well_separated(spec, real);
    return v;
}

nlohmann::json verdict_json(const Verdict& v) {
    nlohmann::json j;
    if (v.realizable.has_value()) {
        j["realizable"] = *v.realizable;
    } else {
        j["realizable"] = nullptr;
    }
    j["well_separated"] = v.well_separated;
    j["kept"] = v.kept();
    return j;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace

int main(int argc, const char* const argv[]) {
    std::size_t parallel = 4;
    std::int64_t timeout_ms = 30000;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--parallel=", 0) == 0) {
            parallel = static_cast<std::size_t>(std::stoul(arg.substr(11)));
        } else if (arg.rfind("--timeout-ms=", 0) == 0) {
            timeout_ms = std::stoll(arg.substr(13));
        } else {
            paths.push_back(arg);
        }
    }
    if (paths.empty()) {
        std::cerr << "usage: degrees [--parallel=N] [--timeout-ms=N] "
                     "<spec.tlsf>...\n";
        return 1;
    }

    set_thread_pool_size(parallel);
    RealizabilityChecker::set_max_concurrency(parallel);
    RealizabilityChecker::set_timeout(std::chrono::milliseconds(timeout_ms));
    RealizabilityChecker& real = global_real_checker();
    const std::size_t in_flight = dispatch_window();

    for (const std::string& path : paths) {
        const std::string family =
            std::filesystem::path(path).parent_path().filename().string();
        tlsf::Specification spec;
        try {
            spec = tlsf::parse(read_file(path));
        } catch (const std::exception& exc) {
            std::cerr << path << ": " << exc.what() << "\n";
            continue;
        }

        const auto started = std::chrono::steady_clock::now();
        const std::vector<tlsf::CoreFormula> parts =
            tlsf::split_guarantee_parts(spec);
        const std::size_t n = parts.size();

        nlohmann::json out;
        out["family"] = family;
        out["path"] = path;
        out["n_parts"] = n;
        out["whole"] = verdict_json(ask(spec, real));

        std::vector<Verdict> solo(n);
        run_bounded_async(
            n, in_flight,
            [&](std::size_t i) {
                return [&, i] {
                    return ask(tlsf::build_part_subset(
                                   spec, parts, std::vector<std::size_t>{i}),
                               real);
                };
            },
            [&solo](std::size_t i, Verdict v) { solo[i] = std::move(v); });

        std::vector<std::pair<std::size_t, std::size_t>> pairs;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                pairs.emplace_back(i, j);
            }
        }
        std::vector<Verdict> pair_verdict(pairs.size());
        run_bounded_async(
            pairs.size(), in_flight,
            [&](std::size_t k) {
                const std::size_t left = pairs[k].first;
                const std::size_t right = pairs[k].second;
                return [&, left, right] {
                    return ask(
                        tlsf::build_part_subset(
                            spec, parts, std::vector<std::size_t>{left, right}),
                        real);
                };
            },
            [&pair_verdict](std::size_t k, Verdict v) {
                pair_verdict[k] = std::move(v);
            });

        // Degree exactly as conflict_degree_order counts it: a pair that is
        // not kept (unrealizable, undecided, or ill-separated) is a conflict.
        std::vector<std::size_t> degree(n, 0);
        std::vector<std::size_t> undecided_pairs(n, 0);
        nlohmann::json conflicts = nlohmann::json::array();
        for (std::size_t k = 0; k < pairs.size(); ++k) {
            const Verdict& v = pair_verdict[k];
            if (!v.realizable.has_value()) {
                ++undecided_pairs[pairs[k].first];
                ++undecided_pairs[pairs[k].second];
            }
            if (v.kept()) {
                continue;
            }
            ++degree[pairs[k].first];
            ++degree[pairs[k].second];
            nlohmann::json c;
            c["i"] = pairs[k].first;
            c["j"] = pairs[k].second;
            c["undecided"] = !v.realizable.has_value();
            conflicts.push_back(std::move(c));
        }

        // Stopped before the walks below: this is the cost
        // conflict_degree_order itself pays.
        out["degree_wall_s"] = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - started)
                                   .count();

        nlohmann::json parts_json = nlohmann::json::array();
        for (std::size_t i = 0; i < n; ++i) {
            nlohmann::json p;
            p["index"] = i;
            p["section"] = tlsf::section_name(parts[i].section_id);
            p["formula"] = parts[i].formula.to_string();
            p["solo"] = verdict_json(solo[i]);
            p["degree"] = degree[i];
            p["undecided_pairs"] = undecided_pairs[i];
            // The rank conflict_degree_order sorts on: a part unrealizable on
            // its own is pushed past every other part.
            p["rank"] = solo[i].kept() ? degree[i] : n + 1;
            parts_json.push_back(std::move(p));
        }
        out["parts"] = std::move(parts_json);
        out["conflicts"] = std::move(conflicts);

        // What the walk actually keeps, under index order and under the degree
        // order these degrees induce. The subset queries are memoised, so this
        // costs at most n more execs per order.
        const auto walk = [&](const std::vector<std::size_t>& admission) {
            std::vector<std::size_t> kept;
            for (const std::size_t part : admission) {
                const auto slot =
                    std::lower_bound(kept.begin(), kept.end(), part);
                const auto inserted = kept.insert(slot, part);
                if (!ask(tlsf::build_part_subset(spec, parts, kept), real)
                         .kept()) {
                    kept.erase(inserted);
                }
            }
            return kept;
        };
        std::vector<std::size_t> index_order(n);
        for (std::size_t i = 0; i < n; ++i) {
            index_order[i] = i;
        }
        std::vector<std::size_t> degree_order = index_order;
        std::sort(degree_order.begin(), degree_order.end(),
                  [&](std::size_t a, std::size_t b) {
                      const std::size_t ra = solo[a].kept() ? degree[a] : n + 1;
                      const std::size_t rb = solo[b].kept() ? degree[b] : n + 1;
                      return std::make_pair(ra, a) < std::make_pair(rb, b);
                  });
        out["degree_order"] = degree_order;
        out["kept_index_order"] = walk(index_order);
        out["kept_degree_order"] = walk(degree_order);
        out["wall_s"] = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - started)
                            .count();
        std::cout << out.dump() << std::endl;
    }
    return 0;
}
