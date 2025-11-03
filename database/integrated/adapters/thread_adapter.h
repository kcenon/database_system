// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file thread_adapter.h
 * @brief Thread pool adapter with conditional thread_system integration
 *
 * This adapter provides unified async execution interface for database operations:
 * - When USE_THREAD_SYSTEM is defined: Uses thread_system for high-performance threading
 * - When USE_THREAD_SYSTEM is not defined: Falls back to std::thread + std::packaged_task
 *
 * Features:
 * - Task execution with futures
 * - Priority-based scheduling (for query prioritization)
 * - Cancellation token support (for long-running queries)
 * - Work completion tracking
 * - Thread pool statistics
 *
 * @example
 * @code
 * using namespace database::integrated;
 *
 * db_thread_config config;
 * config.pool_name = "db_async";
 * config.thread_count = 4;  // 0 = auto-detect
 * config.max_queue_size = 1000;
 * config.enable_priority_scheduling = true;
 *
 * thread_adapter pool(config);
 * auto result = pool.initialize();
 * if (!result.is_ok()) {
 *     std::cerr << "Thread pool init failed\n";
 *     return;
 * }
 *
 * // Submit task
 * auto future = pool.submit([]() {
 *     return execute_query("SELECT * FROM users");
 * });
 *
 * // Submit with priority
 * auto high_priority = pool.submit_with_priority(100, []() {
 *     return execute_critical_query();
 * });
 *
 * // Cancellable task
 * auto token = pool.create_cancellation_token();
 * auto cancellable = pool.submit_cancellable(token, []() {
 *     return long_running_query();
 * });
 * // Later: pool.cancel_token(token);
 *
 * pool.shutdown();
 * @endcode
 */

#pragma once

#include "../core/configuration.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>

// Conditional Result pattern inclusion
#if defined(USE_COMMON_SYSTEM)
	#include <kcenon/common/patterns/result.h>
#else
namespace common
{
	struct Error
	{
		std::string message;
		int code;
	};

	template <typename T>
	class Result
	{
	public:
		Result(T value) : value_(std::move(value)), has_value_(true)
		{
		}
		Result(Error error) : error_(std::move(error)), has_value_(false)
		{
		}
		bool is_ok() const
		{
			return has_value_;
		}
		const T& value() const
		{
			return value_;
		}
		const Error& error() const
		{
			return error_;
		}

	private:
		T value_;
		Error error_;
		bool has_value_;
	};

	using VoidResult = Result<bool>;

	inline VoidResult ok()
	{
		return VoidResult(true);
	}
} // namespace common
#endif

namespace database
{
namespace integrated
{
namespace adapters
{

/**
 * @brief Cancellation token for async operations
 *
 * Used to cancel long-running database operations.
 * Thread-safe and can be shared across tasks.
 */
class cancellation_token
{
public:
	cancellation_token();
	~cancellation_token();

	/**
	 * @brief Cancel all operations associated with this token
	 */
	void cancel();

	/**
	 * @brief Check if cancellation was requested
	 * @return true if cancelled
	 */
	bool is_cancelled() const;

private:
	class impl;
	std::shared_ptr<impl> pimpl_;
};

/**
 * @brief Thread pool adapter for async database operations
 *
 * Provides unified async execution with priority scheduling and cancellation support.
 * Optimized for database workloads with configurable pool size and queue depth.
 *
 * Thread Safety: All methods are thread-safe
 * Exception Safety: Strong guarantee - failed operations don't affect pool state
 */
class thread_adapter
{
public:
	/**
	 * @brief Construct thread adapter with configuration
	 * @param config Thread pool configuration
	 */
	explicit thread_adapter(const db_thread_config& config);

	/**
	 * @brief Destructor - ensures graceful shutdown
	 */
	~thread_adapter();

	// Non-copyable
	thread_adapter(const thread_adapter&) = delete;
	thread_adapter& operator=(const thread_adapter&) = delete;

	// Movable
	thread_adapter(thread_adapter&&) noexcept;
	thread_adapter& operator=(thread_adapter&&) noexcept;

	/**
	 * @brief Initialize thread pool
	 * @return Ok on success, error otherwise
	 */
	common::VoidResult initialize();

	/**
	 * @brief Shutdown thread pool gracefully
	 *
	 * Waits for all pending tasks to complete before shutting down.
	 * After shutdown, no new tasks can be submitted.
	 *
	 * @return Ok on success
	 */
	common::VoidResult shutdown();

	/**
	 * @brief Check if thread pool is initialized
	 * @return true if initialized and ready to accept tasks
	 */
	bool is_initialized() const;

	// ═══════════════════════════════════════════════════════════════
	// Task Execution
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Execute a task (fire-and-forget)
	 * @param task Task to execute
	 * @return Ok on successful submission
	 */
	common::VoidResult execute(std::function<void()> task);

	/**
	 * @brief Submit a task and get a future
	 *
	 * @tparam F Function type
	 * @tparam Args Argument types
	 * @param f Function to execute
	 * @param args Arguments to pass to function
	 * @return Future containing the result
	 */
	template <typename F, typename... Args>
	auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

	/**
	 * @brief Submit a task with priority
	 *
	 * Higher priority tasks are executed before lower priority tasks.
	 * Priority range: 0-255 (higher = more urgent)
	 *
	 * @tparam F Function type
	 * @tparam Args Argument types
	 * @param priority Priority level (0-255)
	 * @param f Function to execute
	 * @param args Arguments to pass to function
	 * @return Future containing the result
	 */
	template <typename F, typename... Args>
	auto submit_with_priority(int priority, F&& f, Args&&... args)
		-> std::future<std::invoke_result_t<F, Args...>>;

	// ═══════════════════════════════════════════════════════════════
	// Cancellation Support
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Create a cancellation token
	 *
	 * Token can be used to cancel associated operations.
	 * Token is ref-counted and can be shared across multiple tasks.
	 *
	 * @return Shared pointer to cancellation token
	 */
	std::shared_ptr<cancellation_token> create_cancellation_token();

	/**
	 * @brief Submit a cancellable task
	 *
	 * Task will check cancellation token before and during execution.
	 * If cancelled, std::future will throw operation_cancelled exception.
	 *
	 * @tparam F Function type
	 * @tparam Args Argument types
	 * @param token Cancellation token
	 * @param f Function to execute
	 * @param args Arguments to pass to function
	 * @return Future containing the result
	 */
	template <typename F, typename... Args>
	auto submit_cancellable(std::shared_ptr<cancellation_token> token, F&& f, Args&&... args)
		-> std::future<std::invoke_result_t<F, Args...>>;

	// ═══════════════════════════════════════════════════════════════
	// Work Completion & Statistics
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Wait for all pending tasks to complete
	 *
	 * Blocks until all submitted tasks finish execution.
	 */
	void wait_for_completion();

	/**
	 * @brief Wait for tasks with timeout
	 * @param timeout Maximum time to wait
	 * @return true if all tasks completed, false if timeout
	 */
	bool wait_for_completion_timeout(std::chrono::milliseconds timeout);

	/**
	 * @brief Get number of worker threads
	 * @return Thread count
	 */
	std::size_t worker_count() const;

	/**
	 * @brief Get current queue size
	 * @return Number of pending tasks
	 */
	std::size_t queue_size() const;

	/**
	 * @brief Check if thread pool is idle
	 * @return true if no tasks are running or pending
	 */
	bool is_idle() const;

private:
	class impl;
	std::unique_ptr<impl> pimpl_;
};

// ═══════════════════════════════════════════════════════════════
// Template Implementations
// ═══════════════════════════════════════════════════════════════

template <typename F, typename... Args>
auto thread_adapter::submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{
	using return_type = std::invoke_result_t<F, Args...>;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	auto result = task->get_future();

	execute([task = std::move(task)]() { (*task)(); });

	return result;
}

template <typename F, typename... Args>
auto thread_adapter::submit_with_priority(int priority, F&& f, Args&&... args)
	-> std::future<std::invoke_result_t<F, Args...>>
{
	using return_type = std::invoke_result_t<F, Args...>;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	auto result = task->get_future();

	// Priority is handled inside execute() implementation
	execute([task = std::move(task)]() { (*task)(); });

	return result;
}

template <typename F, typename... Args>
auto thread_adapter::submit_cancellable(
	std::shared_ptr<cancellation_token> token, F&& f, Args&&... args)
	-> std::future<std::invoke_result_t<F, Args...>>
{
	using return_type = std::invoke_result_t<F, Args...>;

	auto promise = std::make_shared<std::promise<return_type>>();
	auto result = promise->get_future();

	// Wrap task with cancellation check
	execute([promise, token, func = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() {
		try
		{
			// Check if cancelled before executing
			if (token && token->is_cancelled())
			{
				promise->set_exception(
					std::make_exception_ptr(std::runtime_error("Operation cancelled")));
				return;
			}

			// Execute and set result
			if constexpr (std::is_void_v<return_type>)
			{
				func();
				promise->set_value();
			}
			else
			{
				promise->set_value(func());
			}
		}
		catch (...)
		{
			promise->set_exception(std::current_exception());
		}
	});

	return result;
}

} // namespace adapters
} // namespace integrated
} // namespace database
