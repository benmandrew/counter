#pragma once

/// @file thread_pool.hpp
/// @brief Fixed-size worker thread pool and the process-lifetime global pool
///        used by all bounded-async dispatch sites.

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

/// Fixed-size pool of worker threads consuming tasks from a shared queue.
/// Unlike std::async(std::launch::async, ...), submitting a task never pays
/// OS thread creation/teardown cost: workers are spawned once and reused for
/// the lifetime of the pool.
class ThreadPool {
   public:
    explicit ThreadPool(std::size_t n_workers);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F>
    auto submit(F task) -> std::future<std::invoke_result_t<F>> {
        using T = std::invoke_result_t<F>;
        auto task_promise = std::make_shared<std::promise<T>>();
        std::future<T> result = task_promise->get_future();
        {
            std::scoped_lock lock(m_mutex);
            m_tasks.emplace([task = std::move(task), task_promise]() mutable {
                try {
                    if constexpr (std::is_void_v<T>) {
                        task();
                        task_promise->set_value();
                    } else {
                        task_promise->set_value(task());
                    }
                } catch (...) {
                    task_promise->set_exception(std::current_exception());
                }
            });
        }
        m_cv.notify_one();
        return result;
    }

   private:
    void worker_loop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
};

/// Returns the process-lifetime thread pool shared by all bounded-async
/// dispatch sites, sized to the hardware concurrency.
ThreadPool& global_thread_pool();
