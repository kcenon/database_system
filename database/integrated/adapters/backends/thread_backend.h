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
 * @file thread_backend.h
 * @brief Abstract interface for thread pool backends
 *
 * Defines the interface that all thread pool backends must implement.
 * This enables runtime selection of thread implementation without
 * conditional compilation.
 */

#pragma once

#include "../../core/common_result.h"
#include "../../core/configuration.h"

#include <chrono>
#include <cstddef>
#include <functional>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class thread_backend
 * @brief Abstract base class for thread pool backends
 *
 * All thread backends (system, fallback, null) must implement this interface.
 * This enables runtime polymorphism and eliminates conditional compilation.
 */
class thread_backend
{
public:
	virtual ~thread_backend() = default;

	/**
	 * @brief Initialize the thread backend
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult initialize() = 0;

	/**
	 * @brief Shutdown the thread backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult shutdown() = 0;

	/**
	 * @brief Check if backend is initialized
	 * @return true if initialized and ready
	 */
	virtual bool is_initialized() const = 0;

	/**
	 * @brief Execute a task (fire-and-forget)
	 * @param task Task to execute
	 * @return VoidResult::ok() on successful submission
	 */
	virtual common::VoidResult execute(std::function<void()> task) = 0;

	/**
	 * @brief Wait for all pending tasks to complete
	 */
	virtual void wait_for_completion() = 0;

	/**
	 * @brief Wait for completion with timeout
	 * @param timeout Maximum wait time
	 * @return true if all tasks completed, false if timeout
	 */
	virtual bool wait_for_completion_timeout(std::chrono::milliseconds timeout) = 0;

	/**
	 * @brief Get number of worker threads
	 * @return Worker count
	 */
	virtual std::size_t worker_count() const = 0;

	/**
	 * @brief Get current queue size
	 * @return Number of pending tasks
	 */
	virtual std::size_t queue_size() const = 0;

	/**
	 * @brief Check if thread pool is idle
	 * @return true if no pending or running tasks
	 */
	virtual bool is_idle() const = 0;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
