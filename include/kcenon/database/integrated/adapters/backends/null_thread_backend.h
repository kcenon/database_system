// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file null_thread_backend.h
 * @brief Null thread backend (synchronous execution)
 *
 * Provides a no-op thread backend that executes tasks synchronously.
 * Useful for:
 * - Single-threaded testing
 * - Debugging without concurrency
 * - Environments where threading is disabled
 */

#pragma once

#include <kcenon/database/integrated/adapters/backends/thread_backend.h>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class null_thread_backend
 * @brief Synchronous thread backend (no threading)
 *
 * This backend executes all tasks synchronously on the calling thread.
 * Useful for testing and debugging.
 */
class null_thread_backend : public thread_backend
{
public:
	explicit null_thread_backend(const db_thread_config& /*config*/) {}
	~null_thread_backend() override = default;

	common::VoidResult initialize() override
	{
		return common::ok();
	}

	common::VoidResult shutdown() override
	{
		return common::ok();
	}

	bool is_initialized() const override
	{
		return true;
	}

	common::VoidResult execute(std::function<void()> task) override
	{
		// Execute synchronously on calling thread
		if (task)
		{
			task();
		}
		return common::ok();
	}

	void wait_for_completion() override
	{
		// No-op: tasks already completed synchronously
	}

	bool wait_for_completion_timeout(std::chrono::milliseconds /*timeout*/) override
	{
		// No-op: tasks already completed
		return true;
	}

	std::size_t worker_count() const override
	{
		return 0; // No worker threads
	}

	std::size_t queue_size() const override
	{
		return 0; // No queue
	}

	bool is_idle() const override
	{
		return true; // Always idle
	}
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
