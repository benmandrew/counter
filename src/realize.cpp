#include <iostream>
#include <string>
#include <vector>

#include "requirement.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <spec.json> [<spec.json> ...]\n"
              << "\n"
              << "Checks whether FRETISH specification(s) are realisable.\n"
              << "Single file: prints REALIZABLE or UNREALIZABLE.\n"
              << "Multiple files: prints \"<path>: REALIZABLE\" or \"<path>: "
                 "UNREALIZABLE\" per line.\n"
              << "Exits with status 0 on success, or status 1 on error.\n";
}

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    if (has_flag(argc, argv, "-h") || has_flag(argc, argv, "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr) {
            paths.emplace_back(argv[i]);
        }
    }

    if (paths.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    const bool multi = paths.size() > 1;
    RealizabilityChecker& checker = global_real_checker();

    for (const std::string& path : paths) {
        Specification spec;
        try {
            spec = load_specification(path);
        } catch (const std::exception& exc) {
            std::cerr << (multi ? path + ": " : "") << exc.what() << "\n";
            return 1;
        }
        const bool realizable = checker.check_realizability(spec);
        const char* result = realizable ? "REALIZABLE" : "UNREALIZABLE";
        if (multi) {
            std::cout << path << ": " << result << "\n";
        } else {
            std::cout << result << "\n";
        }
    }
    return 0;
}
