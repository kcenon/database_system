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
 * @file null_logger_backend.h
 * @brief Null logger backend (no-op)
 *
 * Provides a no-op logger backend that discards all log messages.
 * Useful for:
 * - Performance-critical sections where logging should be disabled
 * - Unit testing where log output is not desired
 * - Production environments with logging disabled
 */

#pragma once

#include "logger_backend.h"

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class null_logger_backend
 * @brief No-op logger backend
 *
 * This backend discards all log messages. All methods are no-ops.
 * Useful for disabling logging without changing client code.
 */
class null_logger_backend : public logger_backend
{
public:
	/**
	 * @brief Construct null logger backend
	 * @param config Logger configuration (ignored)
	 */
	explicit null_logger_backend(const db_logger_config& /*config*/)
	{
	}

	~null_logger_backend() override = default;

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
		return true; // Always "initialized" (no-op)
	}

	void log(db_log_level /*level*/, const std::string& /*message*/) override
	{
		// No-op: discard log message
	}

	void flush() override
	{
		// No-op: nothing to flush
	}
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
