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
 * @file fallback_thread_backend.h
 * @brief Fallback thread backend using std::thread
 *
 * Provides a simple thread pool implementation using standard C++ threading.
 * Used when thread_system is not available.
 *
 * Features:
 * - Fixed-size thread pool with worker threads
 * - Task queue with FIFO scheduling
 * - Graceful shutdown with task completion
 * - Thread-safe operation
 */

#pragma once

#include "thread_backend.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class fallback_thread_backend
 * @brief Simple thread pool using std::thread
 *
 * This backend provides basic thread pooling when thread_system is unavailable.
 * Uses std::thread and std::queue for task management.
 */
class fallback_thread_backend : public thread_backend
{
public:
	explicit fallback_thread_backend(const db_thread_config& config);
	~fallback_thread_backend() override;

	// Non-copyable, non-movable
	fallback_thread_backend(const fallback_thread_backend&) = delete;
	fallback_thread_backend& operator=(const fallback_thread_backend&) = delete;
	fallback_thread_backend(fallback_thread_backend&&) = delete;
	fallback_thread_backend& operator=(fallback_thread_backend&&) = delete;

	common::VoidResult initialize() override;
	common::VoidResult shutdown() override;
	bool is_initialized() const override;

	common::VoidResult execute(std::function<void()> task) override;
	void wait_for_completion() override;
	bool wait_for_completion_timeout(std::chrono::milliseconds timeout) override;

	std::size_t worker_count() const override;
	std::size_t queue_size() const override;
	bool is_idle() const override;

private:
	/**
	 * @brief Worker thread function
	 */
	void worker_thread();

	const db_thread_config& config_;
	bool initialized_;
	std::atomic<bool> shutdown_requested_;

	std::vector<std::thread> workers_;
	std::queue<std::function<void()>> task_queue_;

	mutable std::mutex queue_mutex_;
	std::condition_variable queue_cv_;

	std::atomic<std::size_t> active_tasks_;
	mutable std::mutex completion_mutex_;
	std::condition_variable completion_cv_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
