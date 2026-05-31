// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file monitoring_backend.h
 * @brief Abstract interface for monitoring backends
 *
 * Defines the interface that all monitoring backends must implement.
 * This enables runtime selection of monitoring implementation without
 * conditional compilation.
 */

#pragma once

#include <kcenon/database/integrated/core/common_result.h>
#include <kcenon/database/integrated/core/configuration.h>

#include <chrono>
#include <string>
#include <unordered_map>

namespace kcenon::database
{
namespace integrated
{
namespace adapters
{

// Forward declare database_metrics from monitoring_adapter.h
struct database_metrics;

namespace backends
{

/**
 * @brief Simple metrics snapshot structure
 */
struct metrics_snapshot
{
	std::unordered_map<std::string, double> gauges;
	std::unordered_map<std::string, uint64_t> counters;
	std::string source_id;
};

/**
 * @brief Health status enumeration
 */
enum class health_status
{
	healthy,
	degraded,
	unhealthy,
	unknown
};

/**
 * @brief Health check result structure
 */
struct health_check_result
{
	health_status status;
	std::string message;
	std::unordered_map<std::string, std::string> metadata;
};

/**
 * @class monitoring_backend
 * @brief Abstract base class for monitoring backends
 *
 * All monitoring backends (system, fallback, null) must implement this interface.
 * This enables runtime polymorphism and eliminates conditional compilation.
 */
class monitoring_backend
{
public:
	virtual ~monitoring_backend() = default;

	/**
	 * @brief Initialize the monitoring backend
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult initialize() = 0;

	/**
	 * @brief Shutdown the monitoring backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult shutdown() = 0;

	/**
	 * @brief Check if backend is initialized
	 * @return true if initialized and ready
	 */
	virtual bool is_initialized() const = 0;

	/**
	 * @brief Record a metric value
	 * @param name Metric name
	 * @param value Metric value
	 * @return VoidResult::ok() on success
	 */
	virtual common::VoidResult record_metric(const std::string& name, double value) = 0;

	/**
	 * @brief Record a metric value with tags
	 * @param name Metric name
	 * @param value Metric value
	 * @param tags Metric tags/labels
	 * @return VoidResult::ok() on success
	 */
	virtual common::VoidResult record_metric(
		const std::string& name, double value,
		const std::unordered_map<std::string, std::string>& tags) = 0;

	/**
	 * @brief Get current metrics snapshot
	 * @return Metrics snapshot on success
	 */
	virtual common::Result<metrics_snapshot> get_metrics() = 0;

	/**
	 * @brief Perform health check
	 * @return Health check result
	 */
	virtual common::Result<health_check_result> check_health() = 0;

	/**
	 * @brief Reset all metrics
	 * @return VoidResult::ok() on success
	 */
	virtual common::VoidResult reset() = 0;

	/**
	 * @brief Record query execution
	 * @param duration Query execution duration
	 * @param success Whether query succeeded
	 */
	virtual void record_query_execution(std::chrono::microseconds duration, bool success) = 0;

	/**
	 * @brief Record connection acquisition
	 */
	virtual void record_connection_acquired() = 0;

	/**
	 * @brief Record connection release
	 */
	virtual void record_connection_released() = 0;

	/**
	 * @brief Update connection pool statistics
	 * @param active Number of active connections
	 * @param idle Number of idle connections
	 * @param total Total pool size
	 */
	virtual void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total) = 0;

	/**
	 * @brief Record transaction begin
	 */
	virtual void record_transaction_begin() = 0;

	/**
	 * @brief Record transaction commit
	 */
	virtual void record_transaction_commit() = 0;

	/**
	 * @brief Record transaction rollback
	 */
	virtual void record_transaction_rollback() = 0;

	/**
	 * @brief Get database-specific metrics
	 * @return Database metrics on success
	 */
	virtual common::Result<database_metrics> get_database_metrics() = 0;

	/**
	 * @brief Export metrics in Prometheus format
	 * @return Prometheus-formatted metrics string
	 */
	virtual std::string export_prometheus_metrics() = 0;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
