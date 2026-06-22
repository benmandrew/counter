#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

[[noreturn]] inline void bench_fail(const std::string& msg) {
    std::cout << "BENCH FAIL: " << msg << "\n";
    std::exit(1);
}
