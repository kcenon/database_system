// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

#include <kcenon/database/integrated/adapters/backends/logger_backend.h>

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
} // namespace kcenon::database
