// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file monitoring_adapter.h
 * @brief Database monitoring adapter with runtime backend selection
 *
 * This adapter provides unified monitoring interface for database operations
 * using the backend pattern for runtime polymorphism.
 *
 * Available backends:
 * - fallback_monitoring_backend: Uses internal metrics storage (default)
 * - null_monitoring_backend: No-op backend for disabling monitoring
 *
 * Features:
 * - Connection pool metrics (active, idle, usage percentage)
 * - Query performance tracking (latency, throughput, success rate)
 * - Transaction monitoring (commits, rollbacks, active transactions)
 * - Health checks and alerting
 * - Prometheus-compatible metrics export
 * - Runtime backend selection (no conditional compilation)
 *
 * @example
 * @code
 * using namespace database::integrated;
 *
 * db_monitoring_config config;
 * config.enable_metrics = true;
 * config.enable_profiling = true;
 * config.enable_health_checks = true;
 * config.metrics_interval = std::chrono::seconds(60);
 * config.enable_prometheus_export = true;
 * config.prometheus_port = 9090;
 *
 * monitoring_adapter monitor(config);
 * auto result = monitor.initialize();
 * if (!result.is_ok()) {
 *     std::cerr << "Monitor init failed: " << result.error().message << "\n";
 *     return;
 * }
 *
 * // Record database operations
 * monitor.record_query_execution(std::chrono::microseconds(1500), true);
 * monitor.record_connection_acquired();
 * monitor.update_pool_stats(15, 5, 20); // 15 active, 5 idle, 20 total
 *
 * // Get metrics
 * auto metrics = monitor.get_database_metrics();
 * if (metrics.is_ok()) {
 *     std::cout << "Active connections: " << metrics.value().active_connections << "\n";
 *     std::cout << "Avg query latency: " << metrics.value().avg_query_latency.count() << "us\n";
 * }
 *
 * monitor.shutdown();
 * @endcode
 */

#pragma once

#include "../core/configuration.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

// Use common Result pattern from shared header
#include "../core/common_result.h"

// Include backend interface for complete types needed by Result<>
#include "backends/monitoring_backend.h"

// Forward declare backend class only
namespace database::integrated::adapters::backends
{
	class monitoring_backend;
}

namespace database
{
namespace integrated
{
namespace adapters
{

/**
 * @brief Monitoring backend type selection
 */
enum class monitoring_backend_type
{
	auto_select,  ///< Automatically select best available backend
	system,       ///< Use monitoring_system (requires HAVE_SYSTEM_MONITORING_BACKEND)
	fallback,     ///< Use internal metrics storage
	null          ///< No-op backend (discard all metrics)
};

/**
 * @brief Database-specific metrics structure
 *
 * Contains comprehensive metrics for database operations, connection pooling,
 * and transaction management.
 */
struct database_metrics
{
	// Connection pool metrics
	std::size_t active_connections{ 0 };	///< Currently active connections
	std::size_t idle_connections{ 0 };		///< Idle connections in pool
	std::size_t total_connections{ 0 };		///< Total pool size
	double connection_usage_percent{ 0.0 }; ///< Percentage of connections in use

	// Query performance metrics
	std::uint64_t total_queries{ 0 };	  ///< Total queries executed
	std::uint64_t successful_queries{ 0 }; ///< Successfully completed queries
	std::uint64_t failed_queries{ 0 };	  ///< Failed queries
	double query_success_rate{ 0.0 };	  ///< Success rate (0.0 to 1.0)

	// Latency metrics
	std::chrono::microseconds avg_query_latency{ 0 }; ///< Average query latency
	std::chrono::microseconds min_query_latency{ 0 }; ///< Minimum query latency
	std::chrono::microseconds max_query_latency{ 0 }; ///< Maximum query latency
	std::chrono::microseconds p95_query_latency{ 0 }; ///< 95th percentile latency
	std::chrono::microseconds p99_query_latency{ 0 }; ///< 99th percentile latency

	// Transaction metrics
	std::uint64_t active_transactions{ 0 };	   ///< Currently active transactions
	std::uint64_t committed_transactions{ 0 };  ///< Total committed transactions
	std::uint64_t rolled_back_transactions{ 0 }; ///< Total rolled-back transactions
	double transaction_commit_rate{ 0.0 };		///< Commit rate (0.0 to 1.0)

	// Throughput metrics
	double queries_per_second{ 0.0 };	   ///< Query throughput
	double transactions_per_second{ 0.0 }; ///< Transaction throughput

	// Timestamp
	std::chrono::system_clock::time_point timestamp; ///< When metrics were collected
};

/**
 * @brief Monitoring adapter for database operations
 *
 * Provides comprehensive monitoring capabilities with runtime backend selection.
 * No longer uses conditional compilation - backend is selected at runtime.
 *
 * Features:
 * - Connection pool monitoring
 * - Query performance tracking with percentiles
 * - Transaction lifecycle tracking
 * - Health checks with configurable thresholds
 * - Prometheus metrics export
 *
 * Thread Safety: Thread-safe with internal synchronization
 */
class monitoring_adapter
{
public:
	/**
	 * @brief Construct monitoring adapter with configuration
	 * @param config Monitoring configuration
	 * @param backend_type Backend type to use (default: auto_select)
	 */
	explicit monitoring_adapter(
		const db_monitoring_config& config,
		monitoring_backend_type backend_type = monitoring_backend_type::auto_select);

	/**
	 * @brief Destructor - ensures graceful shutdown
	 */
	~monitoring_adapter();

	// Non-copyable
	monitoring_adapter(const monitoring_adapter&) = delete;
	monitoring_adapter& operator=(const monitoring_adapter&) = delete;

	// Move constructor only (const reference member prevents move assignment)
	monitoring_adapter(monitoring_adapter&&) noexcept;
	monitoring_adapter& operator=(monitoring_adapter&&) = delete;

	/**
	 * @brief Initialize monitoring system
	 * @return Ok on success, error otherwise
	 */
	common::VoidResult initialize();

	/**
	 * @brief Shutdown monitoring system
	 * @return Ok on success, error otherwise
	 */
	common::VoidResult shutdown();

	/**
	 * @brief Check if monitoring is initialized
	 * @return true if initialized
	 */
	bool is_initialized() const;

	// ═══════════════════════════════════════════════════════════════
	// Monitoring Interface
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Record a metric value
	 * @param name Metric name
	 * @param value Metric value
	 * @return Ok on success
	 */
	common::VoidResult record_metric(const std::string& name, double value);

	/**
	 * @brief Record a metric value with tags
	 * @param name Metric name
	 * @param value Metric value
	 * @param tags Metric tags/labels
	 * @return Ok on success
	 */
	common::VoidResult record_metric(
		const std::string& name, double value,
		const std::unordered_map<std::string, std::string>& tags);

	/**
	 * @brief Get current metrics snapshot
	 * @return Metrics snapshot on success
	 */
	common::Result<backends::metrics_snapshot> get_metrics();

	/**
	 * @brief Perform health check
	 * @return Health check result
	 */
	common::Result<backends::health_check_result> check_health();

	/**
	 * @brief Reset all metrics
	 * @return Ok on success
	 */
	common::VoidResult reset();

	// ═══════════════════════════════════════════════════════════════
	// Database-Specific Monitoring Methods
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief Record query execution
	 * @param duration Query execution duration
	 * @param success Whether query succeeded
	 */
	void record_query_execution(std::chrono::microseconds duration, bool success);

	/**
	 * @brief Record connection acquisition
	 */
	void record_connection_acquired();

	/**
	 * @brief Record connection release
	 */
	void record_connection_released();

	/**
	 * @brief Update connection pool statistics
	 * @param active Number of active connections
	 * @param idle Number of idle connections
	 * @param total Total pool size
	 */
	void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total);

	/**
	 * @brief Record transaction begin
	 */
	void record_transaction_begin();

	/**
	 * @brief Record transaction commit
	 */
	void record_transaction_commit();

	/**
	 * @brief Record transaction rollback
	 */
	void record_transaction_rollback();

	/**
	 * @brief Get database-specific metrics
	 * @return Database metrics on success
	 */
	common::Result<database_metrics> get_database_metrics();

	/**
	 * @brief Export metrics in Prometheus format
	 * @return Prometheus-formatted metrics string
	 */
	std::string export_prometheus_metrics();

private:
	/**
	 * @brief Create appropriate backend based on type
	 */
	static std::unique_ptr<backends::monitoring_backend> create_backend(
		const db_monitoring_config& config,
		monitoring_backend_type backend_type);

	const db_monitoring_config& config_;
	std::unique_ptr<backends::monitoring_backend> backend_; ///< Monitoring backend implementation
};

} // namespace adapters
} // namespace integrated
} // namespace database
