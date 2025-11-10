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
 * @file monitoring_backend.h
 * @brief Abstract interface for monitoring backends
 *
 * Defines the interface that all monitoring backends must implement.
 * This enables runtime selection of monitoring implementation without
 * conditional compilation.
 */

#pragma once

#include "../../core/common_result.h"
#include "../../core/configuration.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace database
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
} // namespace database
