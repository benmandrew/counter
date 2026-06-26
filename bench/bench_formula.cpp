#include <benchmark/benchmark.h>

#include "prop_formula.hpp"

namespace {

static void BenchSyntacticSimilaritySmall(benchmark::State& state) {
    const Formula lhs("(a & b) | c");
    const Formula rhs("a & (b | !c)");
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs.syntactic_similarity(rhs));
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchSyntacticSimilaritySmall)
    ->Name("Syntactic similarity - small formulas (3 variables)");

// Larger formulas to expose scaling in shared_subformulae (O(n*m)).
static void BenchSyntacticSimilarityLarge(benchmark::State& state) {
    const Formula lhs(
        "((a & b) | (c -> d)) & ((e <-> f) | (!g & h)) | (i -> (j & !k))");
    const Formula rhs(
        "((a | b) & (!c -> d)) | ((e <-> g) & (f | h)) & (i <-> (j | k))");
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs.syntactic_similarity(rhs));
    }
}
// NOLINTNEXTLINE(cert-err58-cpp)
BENCHMARK(BenchSyntacticSimilarityLarge)
    ->Name(
        "Syntactic similarity - large formulas (11 variables, O(n*m) "
        "shared_subformulae)");

}  // namespace
