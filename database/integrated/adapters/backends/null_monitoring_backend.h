// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file null_monitoring_backend.h
 * @brief Null monitoring backend (no-op)
 *
 * Provides a no-op monitoring backend that discards all metrics and operations.
 * Useful for:
 * - Performance-critical sections where monitoring overhead should be avoided
 * - Testing where monitoring is not desired
 * - Production environments with monitoring disabled
 */

#pragma once

#include "monitoring_backend.h"
#include "../monitoring_adapter.h"

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class null_monitoring_backend
 * @brief No-op monitoring backend
 *
 * This backend discards all metrics and operations. All methods are no-ops.
 * Useful for disabling monitoring without changing client code.
 */
class null_monitoring_backend : public monitoring_backend
{
public:
	explicit null_monitoring_backend(const db_monitoring_config& /*config*/) {}
	~null_monitoring_backend() override = default;

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
		return true;
	}

	common::VoidResult record_metric(const std::string& /*name*/, double /*value*/) override
	{
		return common::ok();
	}

	common::VoidResult record_metric(
		const std::string& /*name*/, double /*value*/,
		const std::unordered_map<std::string, std::string>& /*tags*/) override
	{
		return common::ok();
	}

	common::Result<metrics_snapshot> get_metrics() override
	{
		return metrics_snapshot{};
	}

	common::Result<health_check_result> check_health() override
	{
		return health_check_result{ health_status::healthy, "Monitoring disabled", {} };
	}

	common::VoidResult reset() override
	{
		return common::ok();
	}

	void record_query_execution(std::chrono::microseconds /*duration*/, bool /*success*/) override
	{
		// No-op
	}

	void record_connection_acquired() override
	{
		// No-op
	}

	void record_connection_released() override
	{
		// No-op
	}

	void update_pool_stats(std::size_t /*active*/, std::size_t /*idle*/, std::size_t /*total*/) override
	{
		// No-op
	}

	void record_transaction_begin() override
	{
		// No-op
	}

	void record_transaction_commit() override
	{
		// No-op
	}

	void record_transaction_rollback() override
	{
		// No-op
	}

	common::Result<database_metrics> get_database_metrics() override
	{
		return database_metrics{};
	}

	std::string export_prometheus_metrics() override
	{
		return "# Monitoring disabled\n";
	}
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
