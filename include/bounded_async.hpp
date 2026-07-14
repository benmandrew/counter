#pragma once

/// @file bounded_async.hpp
/// @brief Bounded-concurrency async dispatch: runs at most N futures in flight
///        at once, collecting whichever completes first to avoid head-of-line
///        blocking on slow outliers.

#include <chrono>
#include <cstddef>
#include <deque>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>

/// Runs up to `max_in_flight` futures concurrently across `n_items` tasks.
///
/// `spawn(i)` launches the i-th task and must return a std::future. Once
/// `max_in_flight` tasks are outstanding, the next spawn blocks until *any*
/// one of them completes (not necessarily the oldest), and `on_complete` is
/// invoked with that task's index and result (omitted for std::future<void>).
/// Collecting whichever task finishes first, rather than strictly in
/// submission order, prevents a single slow outlier from stalling progress
/// on tasks that already finished.
template <typename Spawn, typename OnComplete>
void run_bounded_async(std::size_t n_items, std::size_t max_in_flight,
                       Spawn spawn, OnComplete on_complete) {
    using Future = decltype(spawn(std::size_t{0}));
    using T = decltype(std::declval<Future&>().get());
    using InFlight = std::deque<std::pair<std::size_t, Future>>;
    InFlight in_flight;

    // Spawned tasks capture references to caller-owned data, and these futures
    // are promise-backed, so unlike std::async's their destructor does not
    // wait. An exception escaping this function (a task rethrown by get()
    // below) would otherwise unwind past that data and free it while the
    // remaining workers still read it. Waiting for every outstanding task
    // before propagating keeps each worker within the lifetime it references.
    struct DrainGuard {
        InFlight& tasks;
        ~DrainGuard() {
            for (auto& task : tasks) {
                if (task.second.valid()) {
                    task.second.wait();
                }
            }
        }
    } drain_guard{in_flight};

    auto collect_one_ready = [&] {
        for (;;) {
            for (auto it = in_flight.begin(); it != in_flight.end(); ++it) {
                if (it->second.wait_for(std::chrono::milliseconds(0)) ==
                    std::future_status::ready) {
                    const std::size_t index = it->first;
                    if constexpr (std::is_void_v<T>) {
                        it->second.get();
                        in_flight.erase(it);
                        on_complete(index);
                    } else {
                        T result = it->second.get();
                        in_flight.erase(it);
                        on_complete(index, std::move(result));
                    }
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    for (std::size_t idx = 0; idx < n_items; ++idx) {
        if (in_flight.size() >= max_in_flight) {
            collect_one_ready();
        }
        in_flight.emplace_back(idx, spawn(idx));
    }
    while (!in_flight.empty()) {
        collect_one_ready();
    }
}
