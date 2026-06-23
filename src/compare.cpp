#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "filter/implication_check.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "serialisation.hpp"

namespace {

struct Args {
    std::string repairs_dir;
    std::vector<std::string> ideal_paths;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --repairs <dir> --ideal <file>"
                 " [--ideal <file>...]\n"
              << "\n"
              << "Compares each repair JSON in <dir> against each ideal\n"
              << "repair and reports whether the found repair is equivalent\n"
              << "to, strictly stronger than, strictly weaker than, or\n"
              << "incomparable with the ideal, under the assume-guarantee\n"
              << "implication order.\n";
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
        } else if (arg == "--ideal" && i + 1 < argc && argv[i + 1] != nullptr) {
            args.ideal_paths.emplace_back(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return std::nullopt;
        }
    }
    if (args.repairs_dir.empty() || args.ideal_paths.empty()) {
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
    Config::black_timeout = std::chrono::milliseconds{20'000};

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
    ideals.reserve(args.ideal_paths.size());
    for (const std::string& path : args.ideal_paths) {
        try {
            ideals.emplace_back(std::filesystem::path(path).filename().string(),
                                load_specification(path));
        } catch (const std::exception& exc) {
            std::cerr << "Error loading ideal " << path << ": " << exc.what()
                      << "\n";
            return 1;
        }
    }

    SatisfiabilityChecker& checker = global_sat_checker();
    std::size_t n_equivalent = 0;
    std::size_t n_stronger = 0;
    std::size_t n_weaker = 0;
    std::size_t n_incomparable = 0;

    // Priority: equivalent > stronger > weaker > incomparable
    enum class Relation : std::uint8_t {
        Incomparable,
        Weaker,
        Stronger,
        Equivalent
    };

    for (const auto& [repair_name, repair_scored] : repairs) {
        Relation best = Relation::Incomparable;
        std::string best_ideal;
        for (const auto& [ideal_name, ideal_spec] : ideals) {
            const bool repair_implies_ideal =
                spec_implies(repair_scored.spec, ideal_spec, checker)
                    .value_or(false);
            const bool ideal_implies_repair =
                spec_implies(ideal_spec, repair_scored.spec, checker)
                    .value_or(false);
            Relation rel;
            if (repair_implies_ideal && ideal_implies_repair) {
                rel = Relation::Equivalent;
            } else if (repair_implies_ideal) {
                rel = Relation::Stronger;
            } else if (ideal_implies_repair) {
                rel = Relation::Weaker;
            } else {
                rel = Relation::Incomparable;
            }
            if (rel > best) {
                best = rel;
                best_ideal = ideal_name;
            }
        }
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
              << n_incomparable << " incomparable\n";
    return 0;
}
