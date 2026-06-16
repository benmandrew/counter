#include "thread_pool.hpp"

#include <utility>

#include "config.hpp"

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
    static ThreadPool pool(Config::n_hw_threads > 0 ? Config::n_hw_threads : 1);
    return pool;
}
