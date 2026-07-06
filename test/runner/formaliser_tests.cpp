#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "runner/formaliser.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// A persistent shell loop standing in for the real `fretCLI.main.js
// formalize --logic ft-inf --batch` process: reads one line, echoes back a
// deterministic transform of it, repeats. Good enough to exercise the
// request/response framing, caching, and concurrency behaviour without
// depending on node or the real (machine-local, temporary-path) tool.
std::vector<std::string> echo_transform_command() {
    return {"/bin/sh", "-c",
            R"(while IFS= read -r line; do printf 'F(%s)\n' "$line"; done)"};
}

void test_basic_request_response() {
    RequirementFormaliser formaliser(echo_transform_command());
    const std::string result = formaliser.formalise("p -> q");
    expect(result == "F(p -> q)",
           "formaliser: expected transformed response for a single request");
}

void test_cache_hit_avoids_second_round_trip() {
    RequirementFormaliser formaliser(echo_transform_command());
    const std::size_t misses_before = RequirementFormaliser::n_cache_misses;
    const std::size_t hits_before = RequirementFormaliser::n_cache_hits;

    const std::string first = formaliser.formalise("G(a -> F b)");
    const std::string second = formaliser.formalise("G(a -> F b)");

    expect(first == "F(G(a -> F b))" && second == first,
           "formaliser: repeated calls should return the same result");
    expect(RequirementFormaliser::n_cache_misses == misses_before + 1,
           "formaliser: only the first call should miss the cache");
    expect(RequirementFormaliser::n_cache_hits == hits_before + 1,
           "formaliser: the second call should hit the cache");
}

void test_concurrent_calls_get_matching_responses() {
    RequirementFormaliser formaliser(echo_transform_command());
    constexpr int n_threads = 8;
    constexpr int n_calls_per_thread = 20;

    std::mutex failures_mutex;
    std::vector<std::string> failures;
    std::vector<std::thread> workers;
    workers.reserve(n_threads);

    for (int thread_idx = 0; thread_idx < n_threads; ++thread_idx) {
        workers.emplace_back([&, thread_idx] {
            for (int call_idx = 0; call_idx < n_calls_per_thread; ++call_idx) {
                const std::string requirement_text =
                    "req-" + std::to_string(thread_idx) + "-" +
                    std::to_string(call_idx);
                const std::string expected = "F(" + requirement_text + ")";
                const std::string actual =
                    formaliser.formalise(requirement_text);
                if (actual != expected) {
                    std::string failure = requirement_text;
                    failure += " -> ";
                    failure += actual;
                    failure += " (expected ";
                    failure += expected;
                    failure += ")";
                    std::scoped_lock lock(failures_mutex);
                    failures.push_back(std::move(failure));
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    expect(failures.empty(),
           "formaliser: concurrent requests must not receive each other's "
           "responses (mismatches: " +
               std::to_string(failures.size()) + ")");
}

void test_unexpected_eof_throws() {
    // Reads exactly one line, then exits without writing a response: the
    // formaliser's write succeeds (the child is still starting up), but the
    // read that follows sees EOF instead of a response line.
    RequirementFormaliser formaliser({"/bin/sh", "-c", "read line; exit 0"});
    bool threw = false;
    try {
        formaliser.formalise("anything");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw,
           "formaliser: a process that closes stdout before responding "
           "should surface as a thrown exception, not a hang");
}

}  // namespace

void run_formaliser_runner_tests() {
    test_basic_request_response();
    test_cache_hit_avoids_second_round_trip();
    test_concurrent_calls_get_matching_responses();
    test_unexpected_eof_throws();
}
