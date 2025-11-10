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

#include "thread_backend.h"

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
} // namespace database
