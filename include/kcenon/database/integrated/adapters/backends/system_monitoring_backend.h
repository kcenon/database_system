// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file system_monitoring_backend.h
 * @brief Monitoring backend using kcenon/monitoring_system
 *
 * Uses the monitoring_system library for production-grade metrics collection:
 * - Performance profiling with percentiles
 * - Prometheus-compatible metrics export
 * - System resource monitoring
 * - Health checks with configurable thresholds
 */

#pragma once

#include <kcenon/database/integrated/adapters/backends/monitoring_backend.h>
#include <kcenon/database/integrated/adapters/monitoring_adapter.h>

#include <memory>
#include <mutex>
#include <chrono>
#include <vector>

// Forward declarations to avoid header dependency when monitoring_system is unavailable
namespace kcenon
{
namespace monitoring
{
	class performance_monitor;
	struct metrics_snapshot;
}
}

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class system_monitoring_backend
 * @brief Monitoring backend using monitoring_system library
 *
 * This backend uses the kcenon/monitoring_system for comprehensive metrics collection.
 * Provides performance profiling, Prometheus export, and health monitoring.
 * Requires monitoring_system to be available at compile time.
 */
class system_monitoring_backend : public monitoring_backend
{
public:
	/**
	 * @brief Construct system monitoring backend
	 * @param config Monitoring configuration
	 */
	explicit system_monitoring_backend(const db_monitoring_config& config);

	~system_monitoring_backend() override;

	// Non-copyable, non-movable (holds unique monitor instance)
	system_monitoring_backend(const system_monitoring_backend&) = delete;
	system_monitoring_backend& operator=(const system_monitoring_backend&) = delete;
	system_monitoring_backend(system_monitoring_backend&&) = delete;
	system_monitoring_backend& operator=(system_monitoring_backend&&) = delete;

	common::VoidResult initialize() override;
	common::VoidResult shutdown() override;
	bool is_initialized() const override;

	common::VoidResult record_metric(const std::string& name, double value) override;
	common::VoidResult record_metric(
		const std::string& name, double value,
		const std::unordered_map<std::string, std::string>& tags) override;

	common::Result<metrics_snapshot> get_metrics() override;
	common::Result<health_check_result> check_health() override;
	common::VoidResult reset() override;

	void record_query_execution(std::chrono::microseconds duration, bool success) override;
	void record_connection_acquired() override;
	void record_connection_released() override;
	void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total) override;
	void record_transaction_begin() override;
	void record_transaction_commit() override;
	void record_transaction_rollback() override;

	common::Result<database_metrics> get_database_metrics() override;
	std::string export_prometheus_metrics() override;

private:
	/**
	 * @brief Convert monitoring_system metrics to database_metrics format
	 */
	database_metrics convert_to_database_metrics(
		const kcenon::monitoring::metrics_snapshot& snapshot);

	/**
	 * @brief Calculate derived metrics from collected data
	 */
	void calculate_derived_metrics();

	const db_monitoring_config& config_;
	bool initialized_;
	mutable std::mutex mutex_;
	std::chrono::steady_clock::time_point start_time_;

	// monitoring_system components
	std::unique_ptr<kcenon::monitoring::performance_monitor> monitor_;

	// Current metrics tracking
	database_metrics current_metrics_;
	std::vector<std::chrono::microseconds> recent_query_latencies_;
	std::size_t max_latency_samples_;

	// Connection pool tracking
	std::size_t active_connections_;
	std::size_t idle_connections_;
	std::size_t total_connections_;

	// Transaction tracking
	std::size_t active_transactions_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
