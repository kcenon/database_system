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
 * @file configuration.h
 * @brief Unified configuration for integrated database system
 *
 * This file provides a comprehensive configuration system using builder pattern
 * for easy setup of database, connection pool, threading, logging, and monitoring
 * subsystems. All configuration structures have smart defaults allowing zero-config
 * usage while still providing full customization capabilities.
 *
 * @example
 * @code
 * using namespace database::integrated;
 *
 * // Zero-config usage with defaults
 * unified_db_config config_simple;
 *
 * // Custom configuration with builder pattern
 * auto config = unified_db_config{}
 *     .set_backend(backend_type::postgres, "host=localhost dbname=mydb")
 *     .set_pool_size(5, 20)
 *     .set_log_level(db_log_level::info)
 *     .enable_monitoring(true)
 *     .set_thread_count(4);
 * @endcode
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace database
{
namespace integrated
{

/**
 * @brief Database logging level enumeration
 *
 * Defines the verbosity level for database operation logging.
 * Lower levels include higher levels (e.g., debug includes info, warning, error).
 */
enum class db_log_level
{
	trace,    ///< Most verbose, includes all operations
	debug,    ///< Debug information for development
	info,     ///< Informational messages (default)
	warning,  ///< Warning conditions
	error,    ///< Error conditions
	critical, ///< Critical failures requiring immediate attention
	fatal     ///< Fatal errors causing system shutdown
};

/**
 * @brief Database backend type enumeration
 *
 * Specifies which database backend to use.
 */
enum class backend_type
{
	postgres, ///< PostgreSQL database
	mysql,    ///< MySQL/MariaDB database
	sqlite,   ///< SQLite embedded database
	mongodb,  ///< MongoDB NoSQL database
	redis     ///< Redis key-value store
};

/**
 * @brief Thread pool implementation type
 *
 * Determines which thread pool implementation to use for async operations.
 */
enum class thread_pool_type
{
	standard, ///< Standard thread pool (default)
	typed     ///< Typed thread pool with priority support
};

/**
 * @brief Connection pool configuration
 *
 * Controls the behavior of the database connection pool including sizing,
 * timeouts, and health checking.
 */
struct pool_config
{
	/// Unique name for this pool (for logging and monitoring)
	std::string pool_name{ "default_pool" };

	/// Minimum number of connections to maintain
	std::size_t min_connections{ 2 };

	/// Maximum number of connections allowed
	std::size_t max_connections{ 10 };

	/// Timeout for acquiring a connection from the pool
	std::chrono::seconds connection_timeout{ 30 };

	/// Time before an idle connection is closed
	std::chrono::seconds idle_timeout{ 300 }; // 5 minutes

	/// Enable periodic health checks on connections
	bool enable_health_checks{ true };

	/// Interval between health checks
	std::chrono::seconds health_check_interval{ 60 };

	/// Enable priority-based connection acquisition
	bool enable_priority_queue{ false };
};

/**
 * @brief Thread pool configuration for async operations
 *
 * Configures the thread pool used for asynchronous query execution.
 */
struct db_thread_config
{
	/// Name for this thread pool (for logging and monitoring)
	std::string pool_name{ "db_thread_pool" };

	/// Number of worker threads (0 = auto-detect from hardware)
	std::size_t thread_count{ 0 };

	/// Maximum queued tasks (0 = unlimited)
	std::size_t max_queue_size{ 1000 };

	/// Enable priority-based task scheduling
	bool enable_priority_scheduling{ false };

	/// Thread pool implementation type
	thread_pool_type pool_type{ thread_pool_type::standard };
};

/**
 * @brief Logger configuration
 *
 * Controls database operation logging including query logging,
 * slow query detection, and output destinations.
 */
struct db_logger_config
{
	/// Log all SQL queries executed
	bool enable_query_logging{ false };

	/// Log connection pool events (acquire, release, etc.)
	bool enable_connection_logging{ true };

	/// Automatically detect and log slow queries
	bool log_slow_queries{ true };

	/// Threshold for considering a query "slow"
	std::chrono::milliseconds slow_query_threshold{ 1000 }; // 1 second

	/// Minimum log level to output
	db_log_level min_log_level{ db_log_level::info };

	/// Enable logging to file (in addition to console)
	bool enable_file_logging{ false };

	/// Directory for log files
	std::string log_directory{ "./logs" };

	/// Log file rotation size in bytes (0 = no rotation)
	std::size_t log_rotation_size{ 10 * 1024 * 1024 }; // 10 MB

	/// Number of rotated log files to keep
	std::size_t log_rotation_count{ 5 };
};

/**
 * @brief Monitoring and metrics configuration
 *
 * Enables performance metrics collection, health monitoring,
 * and optional Prometheus integration.
 */
struct db_monitoring_config
{
	/// Enable metrics collection
	bool enable_metrics{ true };

	/// Enable performance profiling
	bool enable_profiling{ false };

	/// Enable health check endpoints
	bool enable_health_checks{ true };

	/// Interval for collecting metrics
	std::chrono::seconds metrics_interval{ 60 };

	/// Warn when connection pool usage exceeds this percentage (0.0-1.0)
	double connection_usage_warning_threshold{ 0.8 }; // 80%

	/// Warn when query latency exceeds this threshold
	std::chrono::milliseconds query_latency_warning{ 500 };

	/// Enable Prometheus metrics export
	bool enable_prometheus_export{ false };

	/// HTTP endpoint for Prometheus scraping
	std::string prometheus_endpoint{ "/metrics" };

	/// Port for Prometheus metrics server
	std::uint16_t prometheus_port{ 9090 };
};

/**
 * @brief Database-specific configuration
 *
 * Contains connection details and database-specific settings.
 */
struct database_config
{
	/// Database backend type
	backend_type type{ backend_type::postgres };

	/// Connection string (format depends on backend)
	std::string connection_string{ "host=localhost port=5432 dbname=postgres" };

	/// Enable SSL/TLS for database connections
	bool enable_ssl{ false };

	/// Path to SSL certificate file
	std::string ssl_cert_path{};

	/// Path to SSL key file
	std::string ssl_key_path{};

	/// Enable prepared statement caching
	bool enable_prepared_statements{ true };

	/// Enable query result caching
	bool enable_query_cache{ false };

	/// Maximum size of query cache in bytes
	std::size_t query_cache_size{ 100 * 1024 * 1024 }; // 100 MB

	/// Database username
	std::string username{};

	/// Database password (stored in memory - consider using secrets management)
	std::string password{};
};

/**
 * @brief Unified database system configuration
 *
 * Combines all subsystem configurations into a single structure
 * with a fluent builder API for easy configuration.
 *
 * @example
 * @code
 * auto config = unified_db_config{}
 *     .set_backend(backend_type::postgres, "host=localhost dbname=test")
 *     .set_credentials("admin", "password123")
 *     .set_pool_size(5, 20)
 *     .set_log_level(db_log_level::debug)
 *     .enable_slow_query_logging(true, std::chrono::milliseconds(500))
 *     .enable_monitoring(true)
 *     .enable_prometheus(true, 9090)
 *     .set_thread_count(4);
 * @endcode
 */
struct unified_db_config
{
	/// Database connection configuration
	database_config database;

	/// Connection pool configuration
	pool_config connection_pool;

	/// Thread pool configuration
	db_thread_config thread;

	/// Logger configuration
	db_logger_config logger;

	/// Monitoring configuration
	db_monitoring_config monitoring;

	// Integration flags (compile-time configurable via CMake)
	/// Enable common_system integration (Result pattern, etc.)
	bool enable_common_system_integration{ true };

	/// Enable thread_system integration (typed thread pools)
	bool enable_thread_system_integration{ true };

	/// Enable logger_system integration
	bool enable_logger_system_integration{ true };

	/// Enable monitoring_system integration
	bool enable_monitoring_system_integration{ true };

	// Builder pattern methods for fluent API

	/**
	 * @brief Set database backend type and connection string
	 * @param type Backend type (postgres, mysql, etc.)
	 * @param connection_str Connection string in backend-specific format
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_backend(backend_type type, const std::string& connection_str)
	{
		database.type = type;
		database.connection_string = connection_str;
		return *this;
	}

	/**
	 * @brief Set database credentials
	 * @param user Database username
	 * @param pass Database password
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_credentials(const std::string& user, const std::string& pass)
	{
		database.username = user;
		database.password = pass;
		return *this;
	}

	/**
	 * @brief Configure connection pool size
	 * @param min Minimum number of connections
	 * @param max Maximum number of connections
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_pool_size(std::size_t min, std::size_t max)
	{
		connection_pool.min_connections = min;
		connection_pool.max_connections = max;
		return *this;
	}

	/**
	 * @brief Set connection pool name
	 * @param name Pool identifier for logging/monitoring
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_pool_name(const std::string& name)
	{
		connection_pool.pool_name = name;
		return *this;
	}

	/**
	 * @brief Set minimum logging level
	 * @param level Minimum level to output
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_log_level(db_log_level level)
	{
		logger.min_log_level = level;
		return *this;
	}

	/**
	 * @brief Enable/disable query logging
	 * @param enable True to log all queries
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_query_logging(bool enable = true)
	{
		logger.enable_query_logging = enable;
		return *this;
	}

	/**
	 * @brief Configure slow query detection
	 * @param enable Enable slow query logging
	 * @param threshold Queries slower than this are logged
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_slow_query_logging(
		bool enable = true, std::chrono::milliseconds threshold = std::chrono::milliseconds(1000))
	{
		logger.log_slow_queries = enable;
		logger.slow_query_threshold = threshold;
		return *this;
	}

	/**
	 * @brief Enable file logging
	 * @param enable Enable logging to files
	 * @param directory Directory for log files
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_file_logging(bool enable = true, const std::string& directory = "./logs")
	{
		logger.enable_file_logging = enable;
		logger.log_directory = directory;
		return *this;
	}

	/**
	 * @brief Enable/disable monitoring
	 * @param enable True to enable metrics collection
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_monitoring(bool enable = true)
	{
		monitoring.enable_metrics = enable;
		monitoring.enable_health_checks = enable;
		return *this;
	}

	/**
	 * @brief Configure Prometheus metrics export
	 * @param enable Enable Prometheus endpoint
	 * @param port Port for metrics server
	 * @param endpoint HTTP endpoint path
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_prometheus(
		bool enable = true, std::uint16_t port = 9090, const std::string& endpoint = "/metrics")
	{
		monitoring.enable_prometheus_export = enable;
		monitoring.prometheus_port = port;
		monitoring.prometheus_endpoint = endpoint;
		return *this;
	}

	/**
	 * @brief Set thread pool size
	 * @param count Number of worker threads (0 = auto-detect)
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_thread_count(std::size_t count)
	{
		thread.thread_count = count;
		return *this;
	}

	/**
	 * @brief Enable priority-based scheduling
	 * @param enable Enable for both thread pool and connection pool
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_priority_scheduling(bool enable = true)
	{
		thread.enable_priority_scheduling = enable;
		connection_pool.enable_priority_queue = enable;
		if (enable)
		{
			thread.pool_type = thread_pool_type::typed;
		}
		return *this;
	}

	/**
	 * @brief Enable SSL/TLS for database connections
	 * @param enable Enable SSL
	 * @param cert_path Path to certificate file
	 * @param key_path Path to key file
	 * @return Reference to this config for chaining
	 */
	unified_db_config& enable_ssl(bool enable = true, const std::string& cert_path = "",
		const std::string& key_path = "")
	{
		database.enable_ssl = enable;
		database.ssl_cert_path = cert_path;
		database.ssl_key_path = key_path;
		return *this;
	}

	/**
	 * @brief Set connection timeouts
	 * @param acquisition Timeout for acquiring connections
	 * @param idle Timeout before closing idle connections
	 * @return Reference to this config for chaining
	 */
	unified_db_config& set_timeouts(std::chrono::seconds acquisition, std::chrono::seconds idle)
	{
		connection_pool.connection_timeout = acquisition;
		connection_pool.idle_timeout = idle;
		return *this;
	}
};

} // namespace integrated
} // namespace database
