#include "driver_support.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "version.hpp"

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> parse_string_arg(int argc, const char* const* argv,
                                            const char* flag) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            if (argv[i + 1] != nullptr) {
                return std::string(argv[i + 1]);
            }
        }
    }
    return std::nullopt;
}

bool handle_info_flags(int argc, const char* const* argv,
                       void (*print_usage)(const char*)) {
    if (has_flag(argc, argv, "--version")) {
        version::print(std::cout);
        return true;
    }
    if (has_flag(argc, argv, "-h") || has_flag(argc, argv, "--help")) {
        print_usage(argv[0]);
        return true;
    }
    return false;
}

std::vector<std::string> collect_argument_paths(int argc,
                                                const char* const* argv) {
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr) {
            paths.emplace_back(argv[i]);
        }
    }
    return paths;
}

std::optional<std::string> read_file_contents(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}
