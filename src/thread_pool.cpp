#include "thread_pool.hpp"

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

ThreadPool& global_thread_pool() {
    const std::size_t hw_threads = std::thread::hardware_concurrency();
    static ThreadPool pool(hw_threads > 0 ? hw_threads : 1);
    return pool;
}
