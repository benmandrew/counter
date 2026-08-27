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

    /// How many workers this pool runs. Filters use it to size their own
    /// in-flight windows, so that one setting governs concurrency rather than
    /// each site reaching for the hardware concurrency independently.
    [[nodiscard]] std::size_t size() const { return m_workers.size(); }

   private:
    void worker_loop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
};

/// How many workers this process may actually run: the tightest of the hardware
/// concurrency, the CPU affinity mask and the cgroup CPU quota, skipping any
/// bound that cannot be read and never returning 0.
///
/// `std::thread::hardware_concurrency()` alone is wrong under a container.
/// libstdc++ answers it from `sysconf(_SC_NPROCESSORS_ONLN)`, which reports the
/// host's online CPUs and ignores both `--cpuset-cpus` (affinity) and `--cpus`
/// (a cgroup quota), so a pool sized from it oversubscribes a 4-CPU container
/// on a 64-core host 16-fold -- and every worker here also spawns external
/// solver subprocesses. The two extra bounds are Linux-only; elsewhere this is
/// the hardware concurrency, floored at 1.
std::size_t available_parallelism();

/// Sets how many workers global_thread_pool() starts, from Config::parallel.
/// Zero means available_parallelism(), which is also what a caller that never
/// sets it gets. Call once at startup: the pool is built on first use and never
/// resized, so a later call has no effect.
void set_thread_pool_size(std::size_t size);

/// Returns the process-lifetime thread pool shared by all bounded-async
/// dispatch sites, sized by set_thread_pool_size or, failing that, to
/// available_parallelism().
ThreadPool& global_thread_pool();

/// The in-flight window every bounded-async dispatch site should pass to
/// run_bounded_async: twice the global pool's worker count. The window is
/// queue depth ahead of the pool, not extra concurrency -- the pool still runs
/// at most size() tasks at once -- so 2x keeps every worker fed across task
/// completions without queuing unboundedly.
std::size_t dispatch_window();
