// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file logger_adapter.h
 * @brief Database logging adapter with runtime backend selection
 *
 * This adapter provides unified logging interface for database operations
 * using the backend pattern for runtime polymorphism.
 *
 * Available backends:
 * - common_logger_backend: Uses common_system's ILogger and GlobalLoggerRegistry (default)
 * - fallback_logger_backend: Uses std::cout + std::ofstream (when common_system unavailable)
 * - null_logger_backend: No-op backend for disabling logging
 *
 * Features:
 * - SQL query logging with sanitization (password removal, truncation)
 * - Automatic slow query detection and warning
 * - Connection pool event logging
 * - Transaction logging
 * - Error logging with SQL state codes
 * - Thread-safe operation
 * - Runtime backend selection (no conditional compilation)
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
 * // Auto-selects best available backend
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

// Use common Result pattern from shared header
#include "../core/common_result.h"

// Forward declare backend interface
namespace database::integrated::adapters::backends
{
	class logger_backend;
}

namespace database
{
namespace integrated
{
namespace adapters
{

/**
 * @brief Logger backend type selection
 */
enum class logger_backend_type
{
	auto_select,  ///< Automatically select best available backend
	system,       ///< Use common_system ILogger (fails if unavailable)
	fallback,     ///< Use std::cout + std::ofstream
	null          ///< No-op backend (discard all logs)
};

/**
 * @class logger_adapter
 * @brief Unified logging adapter for database operations
 *
 * Provides a consistent logging interface with runtime backend selection.
 * No longer uses conditional compilation - backend is selected at runtime.
 *
 * Thread Safety: All public methods are thread-safe.
 */
class logger_adapter
{
public:
	/**
	 * @brief Construct logger adapter with configuration
	 * @param config Logger configuration settings
	 * @param backend_type Backend type to use (default: auto_select)
	 */
	explicit logger_adapter(
		const db_logger_config& config,
		logger_backend_type backend_type = logger_backend_type::auto_select);

	/**
	 * @brief Destructor - ensures proper shutdown
	 *
	 * Automatically calls shutdown() if still initialized.
	 */
	~logger_adapter();

	// Non-copyable
	logger_adapter(const logger_adapter&) = delete;
	logger_adapter& operator=(const logger_adapter&) = delete;

	// Move constructor only (const reference member prevents move assignment)
	logger_adapter(logger_adapter&&) noexcept;
	logger_adapter& operator=(logger_adapter&&) = delete;

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
	/**
	 * @brief Create appropriate backend based on type
	 */
	static std::unique_ptr<backends::logger_backend> create_backend(
		const db_logger_config& config,
		logger_backend_type backend_type);

	const db_logger_config& config_;
	std::unique_ptr<backends::logger_backend> backend_; ///< Logger backend implementation
};

} // namespace adapters
} // namespace integrated
} // namespace database
