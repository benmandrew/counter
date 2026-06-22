#include <benchmark/benchmark.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

#include "bench_suite.hpp"
#include "bench_support.hpp"
#include "filter/implication_check.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"

namespace {

static void BenchSpecImpliesWarmCache(benchmark::State& state) {
    const Specification original(
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
    const Specification weaker(
        {},
        {
            Requirement(Formula("true"), Formula("takeoff_roll"),
                        timing::for_ticks(5)),
            Requirement(Formula("true"), Formula("lift_off"),
                        timing::within_ticks(5)),
            Requirement(Formula("!(takeoff_roll)"), Formula("lift_off"),
                        timing::after_ticks(1)),
        },
        {"takeoff_roll"}, {"lift_off"});
    SatisfiabilityChecker checker;
    spec_implies(original, weaker, checker);
    spec_implies(weaker, original, checker);
    const auto miss_before =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed);
    for (auto _ : state) {
        benchmark::DoNotOptimize(spec_implies(original, weaker, checker));
    }
    const auto calls =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed) -
        miss_before;
    state.counters["black_calls_per_iter"] = benchmark::Counter(
        static_cast<double>(calls), benchmark::Counter::kAvgIterations);
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchSpecImpliesWarmCache);

// Each pair: same condition/timing/type, unrelated responses (propositional
// SAT). After the authoritative-shortcut fix: 1 black call per pair. Before
// the fix: 2 (propositional SAT fell through to a redundant LTL call).
static bool check_sat_black_calls() {
    constexpr std::size_t k_pairs = 20;
    SatisfiabilityChecker checker;
    const auto miss_before =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed);
    for (std::size_t pair = 0; pair < k_pairs; ++pair) {
        const std::string from_resp = "f" + std::to_string(pair);
        const std::string dest_resp = "d" + std::to_string(pair);
        const Specification from_spec(
            {},
            {Requirement(Formula("true"), Formula(from_resp),
                         timing::for_ticks(5))},
            {}, {});
        const Specification dest_spec(
            {},
            {Requirement(Formula("true"), Formula(dest_resp),
                         timing::for_ticks(5))},
            {}, {});
        spec_implies(from_spec, dest_spec, checker);
    }
    const std::size_t calls =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed) -
        miss_before;
    std::cout << "SAT same-cond/timing: pairs=" << k_pairs
              << " black_calls=" << calls << " expected=" << k_pairs << "\n";
    if (calls != k_pairs) {
        bench_fail("expected " + std::to_string(k_pairs) +
                   " black calls, got " + std::to_string(calls) +
                   " -- redundant LTL call per SAT pair?");
    }
    return true;
}

// Propositional UNSAT pairs (from implies dest): also 1 black call each.
static bool check_unsat_black_calls() {
    constexpr std::size_t k_pairs = 20;
    SatisfiabilityChecker checker;
    const auto miss_before =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed);
    for (std::size_t pair = 0; pair < k_pairs; ++pair) {
        const std::string atom_a = "a" + std::to_string(pair);
        const std::string atom_b = "b" + std::to_string(pair);
        const Specification from_spec(
            {},
            {Requirement(Formula("true"), Formula(atom_a + " & " + atom_b),
                         timing::for_ticks(5))},
            {}, {});
        const Specification dest_spec(
            {},
            {Requirement(Formula("true"), Formula(atom_a),
                         timing::for_ticks(5))},
            {}, {});
        spec_implies(from_spec, dest_spec, checker);
    }
    const std::size_t calls =
        SatisfiabilityChecker::n_cache_misses.load(std::memory_order_relaxed) -
        miss_before;
    std::cout << "UNSAT same-cond/timing: pairs=" << k_pairs
              << " black_calls=" << calls << " expected=" << k_pairs << "\n";
    if (calls != k_pairs) {
        bench_fail("expected " + std::to_string(k_pairs) +
                   " black calls, got " + std::to_string(calls));
    }
    return true;
}

}  // namespace

bool run_assertion_checks() {
    return check_sat_black_calls() && check_unsat_black_calls();
}
