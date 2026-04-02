// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file database_coordinator.h
 * @brief Lifecycle manager for all database system adapters (Phase 5)
 *
 * The database_coordinator manages the initialization and shutdown of all
 * integrated adapters in the correct order:
 *
 * **Initialization Order:**
 * 1. Logger Adapter    - For debugging subsequent initializations
 * 2. Monitoring Adapter - For tracking initialization metrics
 * 3. Thread Adapter    - For async operations
 * 4. (Connection Pool would be next in Phase 6)
 *
 * **Shutdown Order:**
 * - Reverse of initialization to ensure clean teardown
 *
 * This ensures that:
 * - Dependencies are initialized before dependents
 * - Logging is available throughout the lifecycle
 * - Resources are released in safe order
 * - Health checks can aggregate status from all adapters
 *
 * @example
 * @code
 * using namespace database::integrated;
 *
 * // Create configuration
 * unified_db_config config;
 * config.logger.log_level = db_log_level::info;
 * config.logger.enable_file_logging = true;
 * config.monitoring.enable_metrics = true;
 * config.thread.pool_size = 4;
 *
 * // Create and initialize coordinator
 * database_coordinator coordinator(config);
 * auto init_result = coordinator.initialize();
 * if (!init_result.is_ok()) {
 *     std::cerr << "Initialization failed\n";
 *     return;
 * }
 *
 * // Access adapters
 * auto* logger = coordinator.get_logger();
 * auto* monitor = coordinator.get_monitor();
 * auto* thread_pool = coordinator.get_thread_pool();
 *
 * // Use adapters
 * logger->log(db_log_level::info, "Database system initialized");
 * monitor->record_query_execution(std::chrono::microseconds(150), true);
 *
 * // Perform health check
 * auto health = coordinator.check_health();
 * if (health.is_ok() && health.value()) {
 *     std::cout << "System healthy\n";
 * }
 *
 * // Shutdown (automatic in destructor, or explicit)
 * coordinator.shutdown();
 * @endcode
 */

#pragma once

#include "configuration.h"

#include <memory>

// Conditional Result pattern inclusion
#include "common_result.h"

// Forward declarations to avoid circular dependencies
namespace database
{
namespace integrated
{
namespace adapters
{
	class logger_adapter;
	class monitoring_adapter;
	class thread_adapter;
} // namespace adapters

/**
 * @class database_coordinator
 * @brief Manages lifecycle and coordination of all database system adapters
 *
 * This class is responsible for:
 * - Creating all adapter instances
 * - Initializing them in the correct order
 * - Providing access to initialized adapters
 * - Shutting down in reverse order
 * - Aggregating health checks
 */
class database_coordinator
{
public:
	/**
	 * @brief Construct coordinator with configuration
	 * @param config Unified configuration for all adapters
	 */
	explicit database_coordinator(const unified_db_config& config);

	/**
	 * @brief Destructor - ensures graceful shutdown
	 */
	~database_coordinator();

	// Non-copyable
	database_coordinator(const database_coordinator&) = delete;
	database_coordinator& operator=(const database_coordinator&) = delete;

	// Moveable
	database_coordinator(database_coordinator&&) noexcept;
	database_coordinator& operator=(database_coordinator&&) noexcept;

	/**
	 * @brief Initialize all adapters in correct order
	 *
	 * Initialization order:
	 * 1. Logger adapter (for observability)
	 * 2. Monitoring adapter (for metrics)
	 * 3. Thread adapter (for async operations)
	 *
	 * If any initialization fails, previously initialized adapters
	 * are shut down cleanly.
	 *
	 * @return Success or error with details
	 */
	common::VoidResult initialize();

	/**
	 * @brief Shutdown all adapters in reverse order
	 *
	 * Shutdown order (reverse of init):
	 * 1. Thread adapter
	 * 2. Monitoring adapter
	 * 3. Logger adapter (last, for final logging)
	 *
	 * @return Success or error with details
	 */
	common::VoidResult shutdown();

	/**
	 * @brief Check if coordinator is initialized
	 * @return true if initialized, false otherwise
	 */
	bool is_initialized() const;

	/**
	 * @brief Get logger adapter
	 * @return Pointer to logger adapter (nullptr if not initialized)
	 */
	adapters::logger_adapter* get_logger();

	/**
	 * @brief Get monitoring adapter
	 * @return Pointer to monitoring adapter (nullptr if not initialized)
	 */
	adapters::monitoring_adapter* get_monitor();

	/**
	 * @brief Get thread adapter
	 * @return Pointer to thread adapter (nullptr if not initialized)
	 */
	adapters::thread_adapter* get_thread_pool();

	/**
	 * @brief Perform aggregate health check of all adapters
	 *
	 * Checks:
	 * - Logger is functional
	 * - Monitoring is collecting metrics
	 * - Thread pool is accepting tasks
	 *
	 * @return true if all adapters are healthy
	 */
	common::Result<bool> check_health();

	/**
	 * @brief Get coordinator statistics
	 *
	 * @return Statistics including initialization time, uptime, etc.
	 */
	struct coordinator_stats
	{
		bool is_initialized;
		bool logger_healthy;
		bool monitoring_healthy;
		bool thread_pool_healthy;
		std::chrono::milliseconds uptime;
		std::chrono::system_clock::time_point init_time;
	};

	common::Result<coordinator_stats> get_stats() const;

private:
	/**
	 * @brief PIMPL idiom for ABI stability
	 */
	class impl;
	std::unique_ptr<impl> pimpl_;
};

} // namespace integrated
} // namespace database
