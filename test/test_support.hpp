#pragma once

#include <stdexcept>
#include <string>

[[noreturn]] inline void fail(const std::string& message) {
    throw std::runtime_error(message);
}

inline void expect(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}
