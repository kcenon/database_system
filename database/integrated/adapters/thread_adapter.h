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
 * @brief Thread pool adapter with runtime backend selection
 *
 * This adapter provides unified async execution interface for database operations
 * using the backend pattern for runtime polymorphism.
 *
 * Available backends:
 * - fallback_thread_backend: Uses std::thread pool (default)
 * - null_thread_backend: Synchronous execution (no threading)
 *
 * Features:
 * - Task execution with futures
 * - Work completion tracking
 * - Thread pool statistics
 * - Runtime backend selection (no conditional compilation)
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
 * // Wait for completion
 * future.wait();
 *
 * pool.shutdown();
 * @endcode
 */

#pragma once

#include "../core/configuration.h"
#include "../../core/concepts.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>

// Use common Result pattern from shared header
#include "../core/common_result.h"

// Forward declare backend interface
namespace database::integrated::adapters::backends
{
	class thread_backend;
}

namespace database
{
namespace integrated
{
namespace adapters
{

/**
 * @brief Thread backend type selection
 */
enum class thread_backend_type
{
	auto_select,  ///< Automatically select best available backend
	fallback,     ///< Use std::thread pool
	null          ///< Synchronous execution (no threading)
};

/**
 * @brief Thread pool adapter for async database operations
 *
 * Provides unified async execution with runtime backend selection.
 * No longer uses conditional compilation - backend is selected at runtime.
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
	 * @param backend_type Backend type to use (default: auto_select)
	 */
	explicit thread_adapter(
		const db_thread_config& config,
		thread_backend_type backend_type = thread_backend_type::auto_select);

	/**
	 * @brief Destructor - ensures graceful shutdown
	 */
	~thread_adapter();

	// Non-copyable
	thread_adapter(const thread_adapter&) = delete;
	thread_adapter& operator=(const thread_adapter&) = delete;

	// Move constructor only (const reference member prevents move assignment)
	thread_adapter(thread_adapter&&) noexcept;
	thread_adapter& operator=(thread_adapter&&) = delete;

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
	 * @tparam F Function type - constrained by SubmittableTask concept
	 * @tparam Args Argument types
	 * @param f Function to execute
	 * @param args Arguments to pass to function
	 * @return Future containing the result
	 *
	 * Uses SubmittableTask concept for compile-time validation:
	 * - Ensures F is invocable with Args...
	 * - Ensures F is move-constructible for async storage
	 */
	template <typename F, typename... Args>
		requires concepts::SubmittableTask<F, Args...>
	auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

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
	/**
	 * @brief Create appropriate backend based on type
	 */
	static std::unique_ptr<backends::thread_backend> create_backend(
		const db_thread_config& config,
		thread_backend_type backend_type);

	const db_thread_config& config_;
	std::unique_ptr<backends::thread_backend> backend_; ///< Thread backend implementation
};

// ═══════════════════════════════════════════════════════════════
// Template Implementations
// ═══════════════════════════════════════════════════════════════

template <typename F, typename... Args>
	requires concepts::SubmittableTask<F, Args...>
auto thread_adapter::submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{
	using return_type = std::invoke_result_t<F, Args...>;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	auto result = task->get_future();

	execute([task = std::move(task)]() { (*task)(); });

	return result;
}

} // namespace adapters
} // namespace integrated
} // namespace database
