#include <benchmark/benchmark.h>

#include <functional>
#include <utility>
#include <vector>

#include "requirement.hpp"

namespace {

Specification takeoff_spec() {
    return Specification(
        {},
        {
            Requirement(Formula("true"), Formula("takeoff_roll"),
                        timing::for_ticks(5)),
            Requirement(Formula("true"), Formula("!(takeoff_roll) & lift_off"),
                        timing::within_ticks(5)),
            Requirement(Formula("!(takeoff_roll)"), Formula("lift_off"),
                        timing::after_ticks(1)),
        },
        {"takeoff_roll"}, {"lift_off"});
}

// Copying is the dominant in-process cost of a generation: the population is
// copied through the filter chain, the selection pool and the breeding step.
static void BenchCopyFormula(benchmark::State& state) {
    const Formula formula("((a & b) | (c -> d)) & ((e <-> f) | (!g & h))");
    for (auto _ : state) {
        Formula copy = formula;
        benchmark::DoNotOptimize(copy);
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchCopyFormula)->Name("Copy formula - 8 variables");

static void BenchCopySpecification(benchmark::State& state) {
    const Specification spec = takeoff_spec();
    for (auto _ : state) {
        Specification copy = spec;
        benchmark::DoNotOptimize(copy);
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchCopySpecification)
    ->Name("Copy specification - 3-guarantee takeoff spec");

// Keyed on the whole specification, so this runs once per fitness-cache probe
// and once per candidate in each deduplication pass.
static void BenchHashSpecification(benchmark::State& state) {
    const Specification spec = takeoff_spec();
    for (auto _ : state) {
        benchmark::DoNotOptimize(std::hash<Specification>{}(spec));
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchHashSpecification)
    ->Name("Hash specification - 3-guarantee takeoff spec");

// Equality on two distinct-but-equal specifications, the case the fitness
// cache and the deduplication filter hit: no pointer identity to short-circuit
// on, so the arenas are compared element-wise.
static void BenchEqualSpecification(benchmark::State& state) {
    const Specification lhs = takeoff_spec();
    const Specification rhs = takeoff_spec();
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs == rhs);
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchEqualSpecification)
    ->Name("Compare specifications - equal, distinct arenas");

}  // namespace
