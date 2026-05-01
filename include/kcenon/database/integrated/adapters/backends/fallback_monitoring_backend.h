// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file fallback_monitoring_backend.h
 * @brief Fallback monitoring backend using internal metrics tracking
 *
 * Used when monitoring_system is not available. Provides basic monitoring
 * functionality with internal metrics storage.
 *
 * Features:
 * - In-memory metrics storage
 * - Thread-safe operation with mutex
 * - Query latency percentile calculations
 * - Health check with configurable thresholds
 */

#pragma once

#include <kcenon/database/integrated/adapters/backends/monitoring_backend.h>
#include <kcenon/database/integrated/adapters/monitoring_adapter.h>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class fallback_monitoring_backend
 * @brief Basic monitoring backend using internal metrics storage
 *
 * This backend provides simple monitoring when monitoring_system is unavailable.
 * Stores metrics in memory and provides basic health checks.
 */
class fallback_monitoring_backend : public monitoring_backend
{
public:
	explicit fallback_monitoring_backend(const db_monitoring_config& config);
	~fallback_monitoring_backend() override;

	// Non-copyable, non-movable
	fallback_monitoring_backend(const fallback_monitoring_backend&) = delete;
	fallback_monitoring_backend& operator=(const fallback_monitoring_backend&) = delete;
	fallback_monitoring_backend(fallback_monitoring_backend&&) = delete;
	fallback_monitoring_backend& operator=(fallback_monitoring_backend&&) = delete;

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
	 * @brief Calculate percentile from latency samples
	 */
	std::chrono::microseconds calculate_percentile(double percentile);

	/**
	 * @brief Update average query latency
	 */
	void update_avg_latency();

	const db_monitoring_config& config_;
	bool initialized_;
	mutable std::mutex mutex_;
	std::chrono::steady_clock::time_point start_time_;

	// Metrics storage
	database_metrics metrics_;
	std::vector<std::chrono::microseconds> query_latencies_;
	std::unordered_map<std::string, double> generic_metrics_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
