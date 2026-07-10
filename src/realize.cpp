#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "requirement.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " <spec.json|spec.tlsf> [...]\n"
        << "\n"
        << "Checks whether specification(s) are realisable. FRETISH JSON\n"
        << "and basic-TLSF (.tlsf) inputs are both accepted; the format\n"
        << "is chosen from the file extension.\n"
        << "Single file: prints REALIZABLE or UNREALIZABLE.\n"
        << "Multiple files: prints \"<path>: REALIZABLE\" or \"<path>: "
           "UNREALIZABLE\" per line.\n"
        << "Exits with status 0 on success, or status 1 on error.\n";
}

std::optional<bool> check_tlsf_realizable(const std::string& path,
                                          RealizabilityChecker& checker) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << path << ": cannot read file\n";
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    try {
        const tlsf::Specification spec = tlsf::parse(contents.str());
        return checker.check_realizability_ltl(spec.to_ltl(), spec.m_inputs,
                                               spec.m_outputs);
    } catch (const std::exception& exc) {
        std::cerr << path << ": " << exc.what() << "\n";
        return std::nullopt;
    }
}

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

// Resolves a single path's realizability, dispatching on the .tlsf extension.
// Returns nullopt (after printing the error) if the file cannot be loaded.
std::optional<bool> realize_one(const std::string& path,
                                RealizabilityChecker& checker, bool multi) {
    if (std::filesystem::path(path).extension() == ".tlsf") {
        return check_tlsf_realizable(path, checker);
    }
    Specification spec;
    try {
        spec = load_specification(path);
    } catch (const std::exception& exc) {
        std::cerr << (multi ? path + ": " : "") << exc.what() << "\n";
        return std::nullopt;
    }
    return checker.check_realizability(spec);
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
        const std::optional<bool> realizable =
            realize_one(path, checker, multi);
        if (!realizable.has_value()) {
            return 1;
        }
        const char* result = *realizable ? "REALIZABLE" : "UNREALIZABLE";
        if (multi) {
            std::cout << path << ": " << result << "\n";
        } else {
            std::cout << result << "\n";
        }
    }
    return 0;
}
