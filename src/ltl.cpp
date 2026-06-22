#include <iostream>
#include <string>
#include <vector>

#include "requirement.hpp"
#include "serialisation.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <spec.json> [<spec.json> ...]\n"
              << "\n"
              << "Prints the LTL formulae for each requirement in a "
                 "specification.\n";
}

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

void print_spec_ltl(const std::string& path, const Specification& spec,
                    bool show_path) {
    if (show_path) {
        std::cout << path << ":\n";
    }
    for (const Requirement& req : spec.m_assumptions) {
        std::cout << "  [assumption] " << req.to_string() << "\n";
        if (req.m_ltl.has_value()) {
            std::cout << "    LTL: " << *req.m_ltl << "\n";
        }
    }
    for (const Requirement& req : spec.m_guarantees) {
        std::cout << "  [guarantee] " << req.to_string() << "\n";
        if (req.m_ltl.has_value()) {
            std::cout << "    LTL: " << *req.m_ltl << "\n";
        }
    }
    if (show_path) {
        std::cout << "\n";
    }
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

    for (const std::string& path : paths) {
        Specification spec;
        try {
            spec = load_specification(path);
        } catch (const std::exception& exc) {
            std::cerr << path << ": " << exc.what() << "\n";
            return 1;
        }
        print_spec_ltl(path, spec, multi);
    }
    return 0;
}
