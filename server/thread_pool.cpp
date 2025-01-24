#include <iostream>
#include <thread_pool.h>


/**
 * @brief Constructs a thread pool with the specified number of threads.
 * 
 * This constructor initializes the thread pool by creating the specified number of worker threads.
 * Each worker thread continuously retrieves and executes tasks from the task queue.
 * 
 * @param thread_count The number of threads to create in the thread pool.
 */
ThreadPool::ThreadPool(size_t thread_count) : stop(false) {
    for (size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }
}


/**
 * @brief Destroys the thread pool and joins all threads.
 * 
 * This destructor stops all worker threads by setting the `stop` flag to `true` and notifying all
 * worker threads. It waits for all threads to finish execution before returning, ensuring no dangling threads.
 */
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std:: thread &worker : workers) {
        worker.join();
    }
}


/**
 * @brief Adds a task to the thread pool's task queue.
 * 
 * This method allows users to submit tasks (functions or callable objects) to the thread pool.
 * The task is added to the task queue and a worker thread is notified to process the task.
 * 
 * @param task A callable object (e.g., `std::function<void()>`) representing the task to be executed.
 */
void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}


/**
 * @brief The main function executed by each worker thread.
 * 
 * This method is the core of the thread pool's worker threads. Each worker continuously retrieves
 * tasks from the task queue and executes them. The worker exits when the `stop` flag is set and
 * the task queue is empty.
 */
void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this]() {return stop || !tasks.empty();});

            if (stop && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}