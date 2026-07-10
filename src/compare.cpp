#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "filter/implication_check.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "serialisation.hpp"
#include "thread_pool.hpp"

namespace {

struct Args {
    std::string repairs_dir;
    std::string ideals_dir;
};

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " --repairs <dir> --ideals <dir>\n"
        << "\n"
        << "Compares each repair JSON in the repairs directory against\n"
        << "every ideal JSON in the ideals directory and reports whether\n"
        << "the found repair is equivalent to, strictly stronger than,\n"
        << "strictly weaker than, or incomparable with the ideal, under\n"
        << "the assume-guarantee implication order.\n";
}

std::optional<Args> parse_args(int argc, const char* const* argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--repairs" && i + 1 < argc &&
                   argv[i + 1] != nullptr) {
            args.repairs_dir = argv[++i];
        } else if (arg == "--ideals" && i + 1 < argc &&
                   argv[i + 1] != nullptr) {
            args.ideals_dir = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return std::nullopt;
        }
    }
    if (args.repairs_dir.empty() || args.ideals_dir.empty()) {
        return std::nullopt;
    }
    return args;
}

std::vector<std::pair<std::string, serialisation::ScoredSpecification>>
load_repairs(const std::string& dir) {
    std::vector<std::pair<std::string, serialisation::ScoredSpecification>>
        repairs;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") {
            continue;
        }
        repairs.emplace_back(entry.path().filename().string(),
                             load_scored_specification(entry.path().string()));
    }
    std::sort(repairs.begin(), repairs.end(),
              [](const auto& lhs, const auto& rhs) {
                  const double lfit =
                      lhs.second.fitness ? lhs.second.fitness->total : 0.0;
                  const double rfit =
                      rhs.second.fitness ? rhs.second.fitness->total : 0.0;
                  return lfit > rfit;
              });
    return repairs;
}

std::vector<std::pair<std::string, Specification>> load_ideals(
    const std::string& dir) {
    std::vector<std::pair<std::string, Specification>> ideals;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") {
            continue;
        }
        ideals.emplace_back(entry.path().filename().string(),
                            load_specification(entry.path().string()));
    }
    std::sort(
        ideals.begin(), ideals.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    return ideals;
}

// Priority: equivalent > stronger > weaker > incomparable > timeout
enum class Relation : std::uint8_t {
    Timeout,
    Incomparable,
    Weaker,
    Stronger,
    Equivalent
};

Relation classify(std::optional<bool> fwd, std::optional<bool> rev) {
    if (fwd.value_or(false) && rev.value_or(false)) {
        return Relation::Equivalent;
    }
    if (fwd.value_or(false)) {
        return Relation::Stronger;
    }
    if (rev.value_or(false)) {
        return Relation::Weaker;
    }
    if (fwd.has_value() && rev.has_value()) {
        return Relation::Incomparable;
    }
    return Relation::Timeout;
}

}  // namespace

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    const auto maybe_args = parse_args(argc, argv);
    if (!maybe_args.has_value()) {
        print_usage(argv[0]);
        return 1;
    }
    const Args& args = *maybe_args;
    const auto has_tlsf_file = [](const std::string& dir) {
        std::error_code err_code;
        const std::filesystem::directory_iterator iter(dir, err_code);
        return std::any_of(std::filesystem::begin(iter),
                           std::filesystem::end(iter), [](const auto& entry) {
                               return entry.path().extension() == ".tlsf";
                           });
    };
    if (std::filesystem::path(args.repairs_dir).extension() == ".tlsf" ||
        std::filesystem::path(args.ideals_dir).extension() == ".tlsf" ||
        has_tlsf_file(args.repairs_dir) || has_tlsf_file(args.ideals_dir)) {
        std::cerr << "compare does not yet support TLSF specifications\n";
        return 1;
    }
    Config cfg;
    cfg.black_timeout = std::chrono::milliseconds{20'000};
    global_sat_checker().set_timeout(cfg.black_timeout);

    std::vector<std::pair<std::string, serialisation::ScoredSpecification>>
        repairs;
    try {
        repairs = load_repairs(args.repairs_dir);
    } catch (const std::exception& exc) {
        std::cerr << "Error loading repairs: " << exc.what() << "\n";
        return 1;
    }
    if (repairs.empty()) {
        std::cerr << "No .json files found in: " << args.repairs_dir << "\n";
        return 1;
    }

    std::vector<std::pair<std::string, Specification>> ideals;
    try {
        ideals = load_ideals(args.ideals_dir);
    } catch (const std::exception& exc) {
        std::cerr << "Error loading ideals: " << exc.what() << "\n";
        return 1;
    }
    if (ideals.empty()) {
        std::cerr << "No .json files found in: " << args.ideals_dir << "\n";
        return 1;
    }

    SatisfiabilityChecker& checker = global_sat_checker();
    std::size_t n_equivalent = 0;
    std::size_t n_stronger = 0;
    std::size_t n_weaker = 0;
    std::size_t n_incomparable = 0;
    std::size_t n_timeout = 0;

    const std::size_t n_repairs = repairs.size();
    const std::size_t n_ideals = ideals.size();
    const std::size_t n_tasks = n_repairs * n_ideals;
    const std::size_t n_hw = std::thread::hardware_concurrency();
    const std::size_t max_in_flight = n_hw > 0 ? n_hw * 2 : 1;

    struct BestResult {
        Relation rel = Relation::Timeout;
        std::size_t ideal_idx = 0;
    };
    std::vector<BestResult> best_per_repair(n_repairs);

    run_bounded_async(
        n_tasks, max_in_flight,
        [&](std::size_t task_idx) {
            const std::size_t rep = task_idx / n_ideals;
            const std::size_t ide = task_idx % n_ideals;
            return global_thread_pool().submit([&, rep, ide] {
                return classify(
                    spec_implies(repairs[rep].second.spec, ideals[ide].second,
                                 checker),
                    spec_implies(ideals[ide].second, repairs[rep].second.spec,
                                 checker));
            });
        },
        [&](std::size_t task_idx, Relation rel) {
            const std::size_t rep = task_idx / n_ideals;
            const std::size_t ide = task_idx % n_ideals;
            if (rel > best_per_repair[rep].rel) {
                best_per_repair[rep] = {rel, ide};
            }
        });

    for (std::size_t idx = 0; idx < n_repairs; ++idx) {
        const auto& [repair_name, repair_scored] = repairs[idx];
        const Relation best = best_per_repair[idx].rel;
        const std::string& best_ideal =
            ideals[best_per_repair[idx].ideal_idx].first;
        std::cout << std::left << std::setw(24) << repair_name << " : ";
        switch (best) {
            case Relation::Equivalent:
                std::cout << "equivalent to " << best_ideal;
                ++n_equivalent;
                break;
            case Relation::Stronger:
                std::cout << "strictly stronger than " << best_ideal;
                ++n_stronger;
                break;
            case Relation::Weaker:
                std::cout << "strictly weaker than " << best_ideal;
                ++n_weaker;
                break;
            case Relation::Incomparable:
                std::cout << "incomparable";
                ++n_incomparable;
                break;
            case Relation::Timeout:
                std::cout << "timeout";
                ++n_timeout;
                break;
        }
        const auto& fitness = repair_scored.fitness;
        if (fitness) {
            const double total = fitness->total;
            std::cout << std::fixed << std::setprecision(4)
                      << "  [fitness: " << total << "]";
        }
        std::cout << "\n";
    }

    std::cout << "\nSummary: " << n_equivalent << " equivalent, " << n_stronger
              << " strictly stronger, " << n_weaker << " strictly weaker, "
              << n_incomparable << " incomparable, " << n_timeout
              << " timeout\n";
    return 0;
}
