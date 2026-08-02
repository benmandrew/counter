#include "thread_pool.hpp"

#include <atomic>
#include <thread>
#include <utility>

ThreadPool::ThreadPool(std::size_t n_workers) {
    m_workers.reserve(n_workers);
    for (std::size_t i = 0; i < n_workers; ++i) {
        m_workers.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::scoped_lock lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (std::thread& worker : m_workers) {
        worker.join();
    }
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_tasks.empty()) {
                if (m_stop) {
                    return;
                }
                continue;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

namespace {

// 0 means "use the hardware concurrency", which is what a run that never calls
// set_thread_pool_size gets. Read once, when the pool below is first touched.
std::atomic<std::size_t> g_pool_size{0};

}  // namespace

void set_thread_pool_size(std::size_t size) { g_pool_size.store(size); }

ThreadPool& global_thread_pool() {
    static ThreadPool pool([] {
        const std::size_t requested = g_pool_size.load();
        if (requested > 0) {
            return requested;
        }
        const std::size_t hw_threads = std::thread::hardware_concurrency();
        return hw_threads > 0 ? hw_threads : 1;
    }());
    return pool;
}
