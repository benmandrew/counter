#include <iostream>
#include <optional>
#include <string>

#include "requirement.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --input <spec.json>\n"
              << "\n"
              << "Checks whether a FRETISH specification is realisable.\n"
              << "Prints REALIZABLE or UNREALIZABLE and exits with status 0\n"
              << "on success, or status 1 on error.\n";
}

std::optional<std::string> parse_input_arg(int argc, const char* const* argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == "--input") {
            if (argv[i + 1] != nullptr) {
                return std::string(argv[i + 1]);
            }
        }
    }
    return std::nullopt;
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
    const std::optional<std::string> input_path = parse_input_arg(argc, argv);
    if (!input_path.has_value()) {
        print_usage(argv[0]);
        return 1;
    }
    Specification spec;
    try {
        spec = load_specification(*input_path);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
    const bool realizable = global_real_checker().check_realizability(spec);
    std::cout << (realizable ? "REALIZABLE" : "UNREALIZABLE") << "\n";
    return 0;
}
