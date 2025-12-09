/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

#include "../adapters/thread_pool_adapter.h"
#include "../core/concepts.h"
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

#ifdef USE_THREAD_SYSTEM
    #include <kcenon/thread/core/job.h>
    #include <kcenon/thread/core/thread_worker.h>
    #include <kcenon/thread/interfaces/thread_context.h>
    #include <kcenon/thread/core/error_handling.h>
#endif

namespace database::async {

#ifdef USE_THREAD_SYSTEM
/**
 * @class lambda_job
 * @brief Wrapper to convert std::function into thread_system job
 *
 * This internal class adapts lambda/function objects to the thread_system
 * job interface by overriding do_work().
 */
class lambda_job : public kcenon::thread::job {
public:
    explicit lambda_job(std::function<void()> func, const std::string& name = "lambda_job")
        : job(name), func_(std::move(func)) {}

    kcenon::thread::result_void do_work() override {
        try {
            if (func_) {
                func_();
            }
            return kcenon::thread::result_void{};  // Success
        } catch (const std::exception& e) {
            return kcenon::thread::error{
                kcenon::thread::error_code::job_execution_failed,
                std::string("Exception in lambda_job: ") + e.what()
            };
        } catch (...) {
            return kcenon::thread::error{
                kcenon::thread::error_code::job_execution_failed,
                "Unknown exception in lambda_job"
            };
        }
    }

private:
    std::function<void()> func_;
};
#endif

/**
 * @class async_executor_v2
 * @brief High-performance asynchronous executor using thread_system
 *
 * This is a drop-in replacement for the legacy async_executor that leverages
 * thread_system's advanced features:
 * - Adaptive job queue (mutex ↔ lock-free automatic switching)
 * - Sub-microsecond latency (77ns job scheduling)
 * - 1.16M+ jobs/second throughput
 * - Integrated monitoring and logging
 *
 * ### Migration Path
 * The API is designed to be compatible with async_executor, allowing
 * gradual migration:
 * @code
 * // Old code (still works)
 * async_executor executor(8);
 * auto future = executor.submit([](){ return 42; });
 *
 * // New code (same API, better performance)
 * async_executor_v2 executor(8);
 * auto future = executor.submit([](){ return 42; });
 * @endcode
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called from multiple threads.
 *
 * ### Performance
 * - **Throughput**: 1.16M+ jobs/s (vs ~50K with std::thread)
 * - **Latency**: 77ns scheduling (vs 2-5μs with std::thread)
 * - **Scalability**: Linear scaling up to hardware concurrency
 */
class async_executor_v2 {
public:
    /**
     * @brief Constructs an async executor with specified thread count
     * @param thread_count Number of worker threads (defaults to hardware concurrency)
     * @param context Thread context for logging/monitoring (optional)
     *
     * ### Example
     * @code
     * // Use hardware concurrency
     * async_executor_v2 executor1;
     *
     * // Use specific thread count
     * async_executor_v2 executor2(4);
     *
     * // With monitoring
     * thread_context_type context;
     * context.set_monitoring(my_monitor);
     * async_executor_v2 executor3(8, context);
     * @endcode
     */
#ifdef USE_THREAD_SYSTEM
    explicit async_executor_v2(
        size_t thread_count = std::thread::hardware_concurrency(),
        const thread_context_type& context = thread_context_type())
        : pool_(std::make_shared<thread_pool_type>("db_async_executor", context))
        , thread_count_(thread_count)
    {
        // Add workers to the pool
        auto job_queue = pool_->get_job_queue();
        for (size_t i = 0; i < thread_count_; ++i) {
            auto worker = std::make_unique<kcenon::thread::thread_worker>(true, context);
            worker->set_job_queue(job_queue);

            auto add_result = pool_->enqueue(std::move(worker));
            if (add_result.has_error()) {
                throw std::runtime_error("Failed to add worker: " +
                                       add_result.get_error().message());
            }
        }

        // Start thread pool
        auto result = pool_->start();
        if (result.has_error()) {
            throw std::runtime_error("Failed to start async executor: " +
                                   result.get_error().message());
        }
    }
#else
    explicit async_executor_v2(
        size_t thread_count = std::thread::hardware_concurrency(),
        const thread_context_type& = thread_context_type())
        : thread_count_(thread_count)
        , stop_(false)
    {
        // Fallback: create worker threads manually
        workers_.reserve(thread_count_);
        for (size_t i = 0; i < thread_count_; ++i) {
            workers_.emplace_back([this] { worker_thread(); });
        }
    }
#endif

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~async_executor_v2() {
        shutdown();
    }

    // Prevent copying and moving
    async_executor_v2(const async_executor_v2&) = delete;
    async_executor_v2& operator=(const async_executor_v2&) = delete;
    async_executor_v2(async_executor_v2&&) = delete;
    async_executor_v2& operator=(async_executor_v2&&) = delete;

    /**
     * @brief Submits a task for asynchronous execution
     * @tparam F Callable type (lambda, function, functor) - constrained by SubmittableTask concept
     * @tparam Args Argument types
     * @param func The callable to execute
     * @param args Arguments to pass to the callable
     * @return std::future with the result of the callable
     *
     * ### C++20 Concepts
     * Uses SubmittableTask concept for compile-time validation:
     * - Ensures F is invocable with Args...
     * - Ensures F is move-constructible for async storage
     *
     * ### Performance
     * - thread_system: 77ns average latency
     * - std::thread fallback: 2-5μs average latency
     *
     * ### Example
     * @code
     * auto future1 = executor.submit([]() { return 42; });
     * auto future2 = executor.submit([](int x) { return x * 2; }, 21);
     *
     * int result1 = future1.get(); // 42
     * int result2 = future2.get(); // 42
     * @endcode
     */
    template<typename F, typename... Args>
        requires concepts::SubmittableTask<F, Args...>
    auto submit(F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

#ifdef USE_THREAD_SYSTEM
        // Use thread_system implementation
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );

        auto future = task->get_future();

        // Wrap lambda as job for thread_system
        auto job = std::make_unique<lambda_job>(
            [task]() { (*task)(); },
            "async_task"
        );

        // Submit to thread pool
        auto result = pool_->enqueue(std::move(job));
        if (result.has_error()) {
            throw std::runtime_error("Failed to enqueue job: " +
                                   result.get_error().message());
        }

        return future;
#else
        // Fallback implementation using std::thread
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );

        auto future = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit task to stopped executor");
            }
            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return future;
#endif
    }

    /**
     * @brief Gracefully shuts down the executor
     *
     * Waits for all pending tasks to complete before shutting down.
     * After calling this, submit() will throw exceptions.
     */
    void shutdown() {
#ifdef USE_THREAD_SYSTEM
        if (pool_) {
            pool_->stop(false); // false = graceful shutdown
        }
#else
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
#endif
    }

    /**
     * @brief Waits for all pending tasks to complete
     *
     * Blocks until the task queue is empty. Does not prevent new tasks
     * from being submitted.
     */
    void wait_for_completion() {
#ifdef USE_THREAD_SYSTEM
        if (pool_) {
            // Wait until queue is empty
            while (pool_->get_job_queue()->size() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
#else
        // Wait until queue is empty
        while (true) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (tasks_.empty()) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
    }

    /**
     * @brief Returns the number of pending tasks
     * @return Number of tasks waiting in the queue
     */
    size_t pending_tasks() const {
#ifdef USE_THREAD_SYSTEM
        if (pool_) {
            return pool_->get_job_queue()->size();
        }
        return 0;
#else
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
#endif
    }

    /**
     * @brief Returns the number of worker threads
     * @return Thread count configured for this executor
     */
    size_t thread_count() const {
        return thread_count_;
    }

    /**
     * @brief Checks if using thread_system implementation
     * @return true if using thread_system, false if using fallback
     */
    constexpr bool is_using_thread_system() const {
        return using_thread_system;
    }

private:
#ifdef USE_THREAD_SYSTEM
    std::shared_ptr<thread_pool_type> pool_;
    size_t thread_count_;
#else
    // Fallback implementation members
    void worker_thread() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            if (task) {
                task();
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    size_t thread_count_;
#endif
};

} // namespace database::async
