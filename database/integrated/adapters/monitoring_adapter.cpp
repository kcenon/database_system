// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/monitoring_adapter.h>
#include <kcenon/database/integrated/adapters/backends/monitoring_backend.h>
#include <kcenon/database/integrated/adapters/backends/null_monitoring_backend.h>
#include <kcenon/database/integrated/adapters/backends/fallback_monitoring_backend.h>

// Include system_monitoring_backend only if monitoring_system is available
#ifdef HAVE_SYSTEM_MONITORING_BACKEND
#include <kcenon/database/integrated/adapters/backends/system_monitoring_backend.h>
#endif

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Backend Factory
// ═══════════════════════════════════════════════════════════════

std::unique_ptr<backends::monitoring_backend> monitoring_adapter::create_backend(
	const db_monitoring_config& config,
	monitoring_backend_type backend_type)
{
	switch (backend_type)
	{
		case monitoring_backend_type::auto_select:
			// Prefer system_monitoring_backend if available
#ifdef HAVE_SYSTEM_MONITORING_BACKEND
			return std::make_unique<backends::system_monitoring_backend>(config);
#else
			return std::make_unique<backends::fallback_monitoring_backend>(config);
#endif

		case monitoring_backend_type::system:
#ifdef HAVE_SYSTEM_MONITORING_BACKEND
			return std::make_unique<backends::system_monitoring_backend>(config);
#else
			// Fallback to fallback backend if system backend not available
			return std::make_unique<backends::fallback_monitoring_backend>(config);
#endif

		case monitoring_backend_type::fallback:
			return std::make_unique<backends::fallback_monitoring_backend>(config);

		case monitoring_backend_type::null:
			return std::make_unique<backends::null_monitoring_backend>(config);

		default:
			return std::make_unique<backends::fallback_monitoring_backend>(config);
	}
}

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

monitoring_adapter::monitoring_adapter(
	const db_monitoring_config& config,
	monitoring_backend_type backend_type)
	: config_(config)
	, backend_(create_backend(config, backend_type))
{
}

monitoring_adapter::~monitoring_adapter()
{
	if (backend_ && backend_->is_initialized())
	{
		backend_->shutdown();
	}
}

monitoring_adapter::monitoring_adapter(monitoring_adapter&&) noexcept = default;

common::VoidResult monitoring_adapter::initialize()
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not created", "" });
	}

	return backend_->initialize();
}

common::VoidResult monitoring_adapter::shutdown()
{
	if (!backend_)
	{
		return common::ok();
	}

	return backend_->shutdown();
}

bool monitoring_adapter::is_initialized() const
{
	return backend_ && backend_->is_initialized();
}

// ═══════════════════════════════════════════════════════════════
// Monitoring Interface Implementation
// ═══════════════════════════════════════════════════════════════

common::VoidResult monitoring_adapter::record_metric(const std::string& name, double value)
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->record_metric(name, value);
}

common::VoidResult monitoring_adapter::record_metric(
	const std::string& name, double value,
	const std::unordered_map<std::string, std::string>& tags)
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->record_metric(name, value, tags);
}

common::Result<backends::metrics_snapshot> monitoring_adapter::get_metrics()
{
	if (!backend_)
	{
		return common::Result<backends::metrics_snapshot>(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->get_metrics();
}

common::Result<backends::health_check_result> monitoring_adapter::check_health()
{
	if (!backend_)
	{
		return common::Result<backends::health_check_result>(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->check_health();
}

common::VoidResult monitoring_adapter::reset()
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->reset();
}

// ═══════════════════════════════════════════════════════════════
// Database-Specific Monitoring
// ═══════════════════════════════════════════════════════════════

void monitoring_adapter::record_query_execution(std::chrono::microseconds duration, bool success)
{
	if (backend_)
	{
		backend_->record_query_execution(duration, success);
	}
}

void monitoring_adapter::record_connection_acquired()
{
	if (backend_)
	{
		backend_->record_connection_acquired();
	}
}

void monitoring_adapter::record_connection_released()
{
	if (backend_)
	{
		backend_->record_connection_released();
	}
}

void monitoring_adapter::update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
{
	if (backend_)
	{
		backend_->update_pool_stats(active, idle, total);
	}
}

void monitoring_adapter::record_transaction_begin()
{
	if (backend_)
	{
		backend_->record_transaction_begin();
	}
}

void monitoring_adapter::record_transaction_commit()
{
	if (backend_)
	{
		backend_->record_transaction_commit();
	}
}

void monitoring_adapter::record_transaction_rollback()
{
	if (backend_)
	{
		backend_->record_transaction_rollback();
	}
}

common::Result<database_metrics> monitoring_adapter::get_database_metrics()
{
	if (!backend_)
	{
		return common::Result<database_metrics>(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->get_database_metrics();
}

std::string monitoring_adapter::export_prometheus_metrics()
{
	if (!backend_)
	{
		return "# Backend not initialized\n";
	}

	return backend_->export_prometheus_metrics();
}

} // namespace adapters
} // namespace integrated
} // namespace database
