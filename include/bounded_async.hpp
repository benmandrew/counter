#pragma once

/// @file bounded_async.hpp
/// @brief Bounded-concurrency dispatch: runs at most N tasks in flight at once,
///        collecting whichever completes first to avoid head-of-line blocking
///        on slow outliers.

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include "profile.hpp"
#include "thread_pool.hpp"

namespace bounded_async_detail {

/// Where finished tasks hand their results back to the dispatcher.
///
/// The dispatcher used to poll every outstanding future with wait_for(0) and
/// sleep 1ms between sweeps, which cost a wake-up per millisecond per dispatch
/// and added up to a millisecond of latency to every completion. Workers now
/// push here and signal, so the dispatcher blocks on a condition variable and
/// wakes when there is genuinely something to collect.
template <typename T>
struct CompletionQueue {
    struct Entry {
        std::size_t m_index{0};
        T m_value{};
        std::exception_ptr m_error;
    };

    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<Entry> m_done;
    std::size_t m_outstanding{0};

    void push(Entry entry) {
        {
            const std::scoped_lock lock(m_mutex);
            m_done.push_back(std::move(entry));
            --m_outstanding;
        }
        m_ready.notify_one();
    }

    Entry pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_ready.wait(lock, [this] { return !m_done.empty(); });
        Entry entry = std::move(m_done.front());
        m_done.pop_front();
        return entry;
    }

    [[nodiscard]] std::size_t in_flight() {
        const std::scoped_lock lock(m_mutex);
        return m_outstanding + m_done.size();
    }
};

/// Void tasks carry no value, but the queue is uniform over T, so they carry
/// this instead of specialising the whole machinery.
struct Unit {};

}  // namespace bounded_async_detail

/// Runs up to @p max_in_flight of @p n_items tasks concurrently on the global
/// thread pool.
///
/// `make_task(i)` returns the callable for item i; its return value is passed
/// to `on_complete(i, value)` (or `on_complete(i)` when it returns void). Once
/// `max_in_flight` tasks are outstanding, launching the next one waits for
/// *any* of them to finish, not the oldest, so one slow outlier cannot stall
/// the collection of tasks that already completed.
///
/// A task that throws has its exception rethrown here, after every other
/// outstanding task has been waited for. Tasks capture references to
/// caller-owned data, so propagating earlier would unwind past that data and
/// free it while the remaining workers still read it.
template <typename MakeTask, typename OnComplete>
void run_bounded_async(std::size_t n_items, std::size_t max_in_flight,
                       MakeTask make_task, OnComplete on_complete) {
    using Task = decltype(make_task(std::size_t{0}));
    using Result = std::invoke_result_t<Task>;
    constexpr bool is_void = std::is_void_v<Result>;
    using Carried =
        std::conditional_t<is_void, bounded_async_detail::Unit, Result>;
    using Queue = bounded_async_detail::CompletionQueue<Carried>;

    // Shared with the workers, and outliving this frame is the point: if an
    // exception escapes below, the drain loop still needs somewhere for the
    // stragglers to report to.
    auto queue = std::make_shared<Queue>();
    if (max_in_flight == 0) {
        max_in_flight = 1;
    }

    auto launch = [&queue, &make_task](std::size_t idx) {
        {
            const std::scoped_lock lock(queue->m_mutex);
            ++queue->m_outstanding;
        }
        global_thread_pool().submit(
            [queue, task = make_task(idx), idx]() mutable {
                typename Queue::Entry entry;
                entry.m_index = idx;
                try {
                    if constexpr (is_void) {
                        task();
                    } else {
                        entry.m_value = task();
                    }
                } catch (...) {
                    entry.m_error = std::current_exception();
                }
                queue->push(std::move(entry));
            });
    };

    auto collect_one = [&queue, &on_complete] {
        COUNTER_PROFILE_SCOPE("dispatch/collect-one-ready");
        typename Queue::Entry entry = queue->pop();
        if (entry.m_error) {
            std::rethrow_exception(entry.m_error);
        }
        if constexpr (is_void) {
            on_complete(entry.m_index);
        } else {
            on_complete(entry.m_index, std::move(entry.m_value));
        }
    };

    // Waits out every task still running, discarding results. Used only when
    // unwinding, so that no worker outlives the data it borrowed.
    struct DrainGuard {
        std::shared_ptr<Queue> queue;
        ~DrainGuard() {
            while (queue->in_flight() > 0) {
                (void)queue->pop();
            }
        }
    } drain_guard{queue};

    for (std::size_t idx = 0; idx < n_items; ++idx) {
        if (queue->in_flight() >= max_in_flight) {
            collect_one();
        }
        launch(idx);
    }
    while (queue->in_flight() > 0) {
        collect_one();
    }
}
