// Tests over the width of the global thread pool: the size a caller asks for,
// the default a caller who asks for nothing gets, and that a second request is
// ignored.
//
// global_thread_pool() is a function-local static, built on first use and never
// resized, so whichever of those a process exercises it can only exercise once.
// That is why the suite branches on COUNTER_TEST_POOL_SIZE rather than testing
// both in sequence, and why CMake registers it twice -- one process per case --
// the same shape as the profile suite's two registrations.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "test_suite.hpp"
#include "test_support.hpp"
#include "thread_pool.hpp"

namespace {

// Returns 0 when the variable is unset or does not parse as a positive count,
// which is the "no size was requested" case rather than a request for zero.
std::size_t requested_pool_size() {
    const char* const value = std::getenv("COUNTER_TEST_POOL_SIZE");
    if (value == nullptr) {
        return 0;
    }
    try {
        const std::int64_t parsed = std::stoll(value);
        return parsed > 0 ? static_cast<std::size_t>(parsed) : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

// The default width, which the sized case must not accidentally ask for.
std::size_t default_pool_size() {
    const unsigned int hw_threads = std::thread::hardware_concurrency();
    return hw_threads > 0 ? hw_threads : 1;
}

// A size that cannot coincide with the default. Asking for exactly the width
// the pool would have taken anyway makes both assertions below pass whatever
// set_thread_pool_size does, and the machine that would expose it is a CI
// runner with as many cores as the number CMake happens to pass in.
std::size_t distinct_from_default(std::size_t requested) {
    return requested == default_pool_size() ? requested + 1 : requested;
}

void test_pool_takes_the_requested_size(std::size_t requested) {
    set_thread_pool_size(requested);
    expect(global_thread_pool().size() == requested,
           "the global pool should run as many workers as were requested");

    // The header promises that a later call has no effect. Nothing enforces
    // that promise but this: g_pool_size is still writable afterwards, so a
    // refactor that read it per call rather than once, at construction, would
    // go unnoticed.
    set_thread_pool_size(requested + 1);
    expect(global_thread_pool().size() == requested,
           "a second set_thread_pool_size call should not resize the pool "
           "already built");
}

void test_pool_defaults_to_the_hardware_concurrency() {
    expect(global_thread_pool().size() == default_pool_size(),
           "a pool nobody sized should run one worker per hardware thread, or "
           "one worker where that is unknown");
}

// size() is only worth asserting on if it describes a pool that runs work, so
// hand it more tasks than it has workers and wait for every one.
void test_every_submitted_task_runs() {
    ThreadPool& pool = global_thread_pool();
    const std::size_t n_tasks = (pool.size() * 4) + 1;

    std::vector<char> ran(n_tasks, 0);
    std::vector<std::future<void>> results;
    results.reserve(n_tasks);
    for (std::size_t i = 0; i < n_tasks; ++i) {
        results.push_back(pool.submit([&ran, i] { ran[i] = 1; }));
    }
    for (std::future<void>& result : results) {
        result.get();
    }

    for (std::size_t i = 0; i < n_tasks; ++i) {
        expect(ran[i] == 1,
               "every task submitted to the pool should have run by the time "
               "its future is ready");
    }
}

}  // namespace

void run_thread_pool_tests() {
    const std::size_t requested = requested_pool_size();
    if (requested > 0) {
        test_pool_takes_the_requested_size(distinct_from_default(requested));
    } else {
        test_pool_defaults_to_the_hardware_concurrency();
    }
    test_every_submitted_task_runs();
}
