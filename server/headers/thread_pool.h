#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>


/**
 * @class ThreadPool
 * @brief A thread pool for managing and executing tasks with a fixed number of threads.
 * 
 * The `ThreadPool` class creates and manages a specified number of worker threads. Tasks 
 * (callable objects) can be enqueued for execution using the `enqueue` method. Each worker 
 * thread retrieves tasks from the queue and executes them. The pool ensures thread-safe 
 * task management and supports graceful shutdown when the pool is destroyed.
 * 
 * Key Features:
 * - Fixed number of threads, determined at construction.
 * - Thread-safe task queue for submitting tasks.
 * - Graceful thread shutdown when the pool is destroyed.
 */
class ThreadPool {
    public:
        ThreadPool(size_t thread_count);
        ~ThreadPool();

    void enqueue(std::function<void()> task);

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;

        void worker();
};

#endif