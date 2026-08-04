#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "runner/ltlfilt.hpp"
#include "runner/process.hpp"
#include "runner/simplify_batcher.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// Per formula, and multiplied by the batch size inside the batcher, so a whole
// batch is generously covered. Long enough that a loaded CI machine cannot turn
// a correctness test into a timeout test.
constexpr std::chrono::milliseconds k_timeout{30000};

// One batcher, so every thread past the first queues behind the same leader and
// the second batch holds most of them. Several batchers would spread the same
// formulae over several execs, each possibly a batch of one, and the test would
// stop exercising batching at all.
constexpr std::size_t k_test_batchers = 1;

std::vector<std::string> sample_formulae() {
    return {"G(p -> F(q))", "p & q",       "q & p",        "G(F(p))",
            "p U q",        "!(!p)",       "X(X(p))",      "p & !p",
            "p | !p",       "G(p) & G(q)", "F(p) | F(q)",  "G(a -> X(b))",
            "a U (b U c)",  "!(a & b)",    "G(!(a & b))",  "F(G(a))",
            "a R b",        "X(a) & X(b)", "G(a) -> G(b)", "(a | b) & c"};
}

// What the batcher has to agree with: the formula-at-a-time exec simplify_ltl
// falls back to.
std::string one_at_a_time(const std::string& binary,
                          const std::string& formula) {
    const ProcessResult result =
        execute_and_capture({binary, "--simplify", "-f", formula}, k_timeout);
    std::string out = result.m_output;
    while (!out.empty() && out.back() == '\n') {
        out.pop_back();
    }
    return out;
}

void test_batch_matches_one_at_a_time(const std::string& binary) {
    const std::vector<std::string> formulae = sample_formulae();
    std::vector<std::optional<std::string>> batched(formulae.size());
    std::vector<std::thread> threads;
    threads.reserve(formulae.size());
    for (std::size_t i = 0; i < formulae.size(); ++i) {
        threads.emplace_back([&binary, &formulae, &batched, i] {
            double child_cpu_s = 0.0;
            batched[i] =
                batched_simplify(binary, formulae[i], child_cpu_s, k_timeout);
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (std::size_t i = 0; i < formulae.size(); ++i) {
        const std::optional<std::string>& got = batched[i];
        if (!got.has_value()) {
            fail("simplify-batcher: batching should resolve \"" + formulae[i] +
                 "\" rather than declining it");
        }
        const std::string expected = one_at_a_time(binary, formulae[i]);
        expect(*got == expected,
               "simplify-batcher: batched \"" + formulae[i] + "\" gave \"" +
                   *got + "\", one at a time gives \"" + expected + "\"");
    }
}

void test_zero_batchers_declines(const std::string& binary) {
    double child_cpu_s = 0.0;
    const std::optional<std::string> result =
        batched_simplify(binary, "G(p -> F(q))", child_cpu_s, k_timeout);
    expect(!result.has_value(),
           "simplify-batcher: ltlfilt_batchers = 0 should decline every "
           "formula, leaving each caller its own exec");
    expect(child_cpu_s == 0.0,
           "simplify-batcher: a declined formula should spawn nothing");
}

// Independent of the pool size, since both are rejected before a batcher is
// ever reached: a blank line is swallowed by ltlfilt without an answer, and a
// formula spanning lines cannot go in a line-oriented batch. Either would break
// the one-answer-per-formula count the batch is checked against.
void test_unbatchable_formulae_decline(const std::string& binary) {
    double child_cpu_s = 0.0;
    expect(!batched_simplify(binary, "  ", child_cpu_s, k_timeout).has_value(),
           "simplify-batcher: a blank formula should not be batched");
    expect(!batched_simplify(binary, "G(p)\n& q", child_cpu_s, k_timeout)
                .has_value(),
           "simplify-batcher: a formula spanning lines should not be batched");
}

}  // namespace

void run_simplify_batcher_tests() {
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return;
    }
    // The pool is built on the first request and never resized, so this has to
    // run before any of the cases below. The two settings therefore need a
    // process each, which is what the second ctest registration buys.
    const char* const disabled = std::getenv("COUNTER_TEST_LTLFILT_BATCHERS");
    const bool batching_off =
        disabled != nullptr && std::string(disabled) == "0";
    set_ltlfilt_batchers(batching_off ? 0 : k_test_batchers);
    test_unbatchable_formulae_decline(binary);
    if (batching_off) {
        test_zero_batchers_declines(binary);
        return;
    }
    test_batch_matches_one_at_a_time(binary);
}
