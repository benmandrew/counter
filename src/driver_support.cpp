#include "driver_support.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
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

std::optional<std::string> find_unknown_arg(
    int argc, const char* const* argv,
    const std::vector<std::string>& value_flags,
    const std::vector<std::string>& bare_flags) {
    auto contains = [](const std::vector<std::string>& flags,
                       const std::string& arg) {
        return std::find(flags.begin(), flags.end(), arg) != flags.end();
    };
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        std::string arg(argv[i]);
        if (contains(value_flags, arg)) {
            // Skip the value, so a path or a seed that happens to look like a
            // flag is not itself reported as unknown.
            ++i;
            continue;
        }
        if (contains(bare_flags, arg)) {
            continue;
        }
        return arg;
    }
    return std::nullopt;
}

std::optional<std::size_t> parse_seed(const std::string& text) {
    // Checked before std::stoull rather than after, because stoull is happy to
    // stop at the first non-digit and to wrap a leading '-' round to the top of
    // the range; neither reports anything the caller could notice.
    const bool all_digits =
        !text.empty() &&
        std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        });
    if (!all_digits) {
        return std::nullopt;
    }
    try {
        const auto value = std::stoull(text);
        if constexpr (sizeof(value) > sizeof(std::size_t)) {
            if (value > std::numeric_limits<std::size_t>::max()) {
                return std::nullopt;
            }
        }
        return static_cast<std::size_t>(value);
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}
