#include <benchmark/benchmark.h>

#include <cstring>
#include <iostream>

#include "bench_suite.hpp"

int main(int argc, char** argv) {
    for (int idx = 1; idx < argc; ++idx) {
        if (std::strcmp(argv[idx], "--check-assertions") == 0) {
            return run_assertion_checks() ? 0 : 1;
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
