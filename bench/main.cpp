#include <benchmark/benchmark.h>

#include <chrono>
#include <cstring>
#include <iostream>

#include "bench_suite.hpp"
#include "runner/black.hpp"

int main(int argc, char** argv) {
    global_sat_checker().set_timeout(std::chrono::seconds(60));
    for (int idx = 1; idx < argc; ++idx) {
        if (std::strcmp(argv[idx], "--check-assertions") == 0) {
            run_assertion_checks();
            return 0;
        }
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
