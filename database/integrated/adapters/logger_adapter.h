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
 * @file logger_adapter.h
 * @brief Database logging adapter with conditional logger_system integration
 *
 * This adapter provides unified logging interface for database operations:
 * - When USE_LOGGER_SYSTEM is defined: Uses kcenon/logger_system for advanced logging
 * - When USE_LOGGER_SYSTEM is not defined: Falls back to std::cout + std::ofstream
 *
 * Features:
 * - SQL query logging with sanitization (password removal, truncation)
 * - Automatic slow query detection and warning
 * - Connection pool event logging
 * - Transaction logging
 * - Error logging with SQL state codes
 * - Thread-safe operation
 *
 * @example
 * @code
 * using namespace database::integrated;
 *
 * db_logger_config config;
 * config.enable_query_logging = true;
 * config.log_slow_queries = true;
 * config.slow_query_threshold = std::chrono::milliseconds(500);
 * config.enable_file_logging = true;
 * config.log_directory = "/var/log/myapp";
 *
 * logger_adapter logger(config);
 * auto result = logger.initialize();
 * if (!result.is_ok()) {
 *     std::cerr << "Logger init failed: " << result.error().message << "\n";
 *     return;
 * }
 *
 * // Log a query
 * logger.log_query(db_log_level::info,
 *     "SELECT * FROM users WHERE id = 123",
 *     std::chrono::microseconds(1500));
 *
 * // Automatic slow query warning
 * logger.log_query(db_log_level::info,
 *     "SELECT * FROM large_table",
 *     std::chrono::microseconds(600000)); // > threshold, auto-warns
 *
 * // Log connection events
 * logger.log_connection_event("acquired", "Pool: main_pool, Priority: high");
 * logger.log_pool_event("resized", 15, 5); // 15 active, 5 idle
 *
 * logger.shutdown();
 * @endcode
 */

#pragma once

#include "../core/configuration.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

// Conditional Result pattern inclusion
#if defined(USE_COMMON_SYSTEM)
	#include <kcenon/common/patterns/result.h>
#else
	// Minimal Result replacement if common_system not available
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
		Result(const T& value)
			: value_(value), has_value_(true)
		{
		}
		Result(const Error& error)
			: error_(error), has_value_(false)
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
	inline VoidResult err(const std::string& msg, int code = -1)
	{
		return VoidResult(Error{ msg, code });
	}
	inline VoidResult error(const std::string& msg, int code = -1)
	{
		return err(msg, code);
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
 * @class logger_adapter
 * @brief Unified logging adapter for database operations
 *
 * Provides a consistent logging interface regardless of whether logger_system
 * is available. Uses PIMPL idiom for ABI stability and to hide implementation
 * details.
 *
 * Thread Safety: All public methods are thread-safe.
 */
class logger_adapter
{
public:
	/**
	 * @brief Construct logger adapter with configuration
	 * @param config Logger configuration settings
	 */
	explicit logger_adapter(const db_logger_config& config);

	/**
	 * @brief Destructor - ensures proper shutdown
	 *
	 * Automatically calls shutdown() if still initialized.
	 */
	~logger_adapter();

	// Non-copyable
	logger_adapter(const logger_adapter&) = delete;
	logger_adapter& operator=(const logger_adapter&) = delete;

	// Movable
	logger_adapter(logger_adapter&&) noexcept;
	logger_adapter& operator=(logger_adapter&&) noexcept;

	/**
	 * @brief Initialize the logger
	 *
	 * Sets up output writers (console, file) and starts the logger.
	 * Must be called before any logging operations.
	 *
	 * @return VoidResult::ok() on success, error on failure
	 */
	common::VoidResult initialize();

	/**
	 * @brief Shutdown the logger gracefully
	 *
	 * Flushes all pending logs and closes files.
	 * Safe to call multiple times.
	 *
	 * @return VoidResult::ok() on success, error on failure
	 */
	common::VoidResult shutdown();

	/**
	 * @brief Check if logger is initialized
	 * @return true if initialized and ready to log
	 */
	bool is_initialized() const;

	// ═══════════════════════════════════════════════════════════════
	// Database-Specific Logging Methods
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Log a SQL query execution
	 *
	 * Automatically sanitizes the query (removes passwords, truncates if long).
	 * If duration exceeds slow_query_threshold, automatically logs as slow query.
	 *
	 * @param level Log level (typically info or debug)
	 * @param query SQL query string (will be sanitized)
	 * @param duration Query execution time
	 */
	void log_query(db_log_level level, const std::string& query, std::chrono::microseconds duration);

	/**
	 * @brief Log a slow query with warning
	 *
	 * Called automatically by log_query() when threshold exceeded,
	 * but can also be called manually.
	 *
	 * @param query SQL query string
	 * @param duration Actual execution time
	 * @param threshold Configured slow query threshold
	 */
	void log_slow_query(const std::string& query, std::chrono::microseconds duration,
		std::chrono::milliseconds threshold);

	/**
	 * @brief Log a connection pool event
	 *
	 * Examples: "acquired", "released", "timeout", "health_check"
	 *
	 * @param event Event type (e.g., "acquired", "released")
	 * @param details Additional details (e.g., "Pool: main_pool, Priority: high")
	 */
	void log_connection_event(const std::string& event, const std::string& details);

	/**
	 * @brief Log a transaction operation
	 *
	 * @param operation Transaction operation (e.g., "begin", "commit", "rollback")
	 * @param success Whether operation succeeded
	 * @param details Additional details (e.g., isolation level, error message)
	 */
	void log_transaction(const std::string& operation, bool success, const std::string& details);

	/**
	 * @brief Log a connection pool state change
	 *
	 * Examples: Pool resize, capacity changes
	 *
	 * @param event Event type (e.g., "resized", "shrunk", "health_check_failed")
	 * @param active Number of active connections
	 * @param idle Number of idle connections
	 */
	void log_pool_event(const std::string& event, std::size_t active, std::size_t idle);

	/**
	 * @brief Log a database error
	 *
	 * @param operation Operation that failed (e.g., "execute_query", "connect")
	 * @param error_msg Error message
	 * @param sql_state SQL state code (e.g., "08006" for connection failure)
	 */
	void log_error(const std::string& operation, const std::string& error_msg,
		const std::string& sql_state = "");

	// ═══════════════════════════════════════════════════════════════
	// Generic Logging
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Generic log message
	 *
	 * @param level Log level
	 * @param message Message to log
	 */
	void log(db_log_level level, const std::string& message);

	/**
	 * @brief Flush pending log messages
	 *
	 * Forces all buffered log messages to be written immediately.
	 * Useful before application exit or critical sections.
	 */
	void flush();

private:
	class impl; ///< PIMPL idiom: Implementation details hidden

	std::unique_ptr<impl> pimpl_; ///< Pointer to implementation
};

} // namespace adapters
} // namespace integrated
} // namespace database
