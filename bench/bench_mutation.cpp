#include <benchmark/benchmark.h>

#include "config.hpp"
#include "genetic/mutation.hpp"
#include "genetic/random_source.hpp"
#include "requirement.hpp"

namespace {

static void BenchMutateSpecification(benchmark::State& state) {
    const Specification spec(
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
    const RandomSource rng = make_random_source_from_seed(42);
    const Config cfg;
    for (auto _ : state) {
        benchmark::DoNotOptimize(mutate_specification(spec, rng, cfg));
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchMutateSpecification)
    ->Name("Mutate specification - 3-guarantee takeoff spec");

}  // namespace
