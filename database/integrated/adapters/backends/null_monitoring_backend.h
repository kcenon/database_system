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
