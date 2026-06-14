// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/backends/fallback_monitoring_backend.h>

#include <kcenon/database/core/result.h>

#include <algorithm>
#include <numeric>
#include <sstream>

namespace
{
	// In-band database_system code (see core/result.h) so the shared
	// common::error_info::code resolves to "DatabaseSystem", not the common
	// (-1..-99) band.
	constexpr int kDatabaseErrorCode =
		static_cast<int>(kcenon::database::error_code::unknown_error);

	inline common::VoidResult make_error(const std::string& msg)
	{
		return common::VoidResult(common::error_info{ kDatabaseErrorCode, msg, "" });
	}

	template<typename T>
	common::Result<T> make_error_result(const std::string& msg)
	{
		return common::Result<T>(common::error_info{ kDatabaseErrorCode, msg, "" });
	}
}

namespace kcenon::database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

fallback_monitoring_backend::fallback_monitoring_backend(const db_monitoring_config& config)
	: config_(config), initialized_(false)
{
}

fallback_monitoring_backend::~fallback_monitoring_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult fallback_monitoring_backend::initialize()
{
	if (initialized_)
	{
		return common::ok();
	}

	start_time_ = std::chrono::steady_clock::now();
	initialized_ = true;
	return common::ok();
}

common::VoidResult fallback_monitoring_backend::shutdown()
{
	if (!initialized_)
	{
		return common::ok();
	}

	std::lock_guard<std::mutex> lock(mutex_);
	metrics_ = database_metrics{};
	query_latencies_.clear();
	generic_metrics_.clear();
	initialized_ = false;
	return common::ok();
}

bool fallback_monitoring_backend::is_initialized() const
{
	return initialized_;
}

common::VoidResult fallback_monitoring_backend::record_metric(const std::string& name, double value)
{
	if (!initialized_)
	{
		return make_error("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	generic_metrics_[name] = value;
	return common::ok();
}

common::VoidResult fallback_monitoring_backend::record_metric(
	const std::string& name, double value,
	const std::unordered_map<std::string, std::string>& /*tags*/)
{
	// Fallback ignores tags
	return record_metric(name, value);
}

common::Result<metrics_snapshot> fallback_monitoring_backend::get_metrics()
{
	if (!initialized_)
	{
		return make_error_result<metrics_snapshot>("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	metrics_snapshot snapshot;
	snapshot.source_id = "database_fallback";

	// Connection metrics
	snapshot.gauges["db.connections.active"] = static_cast<double>(metrics_.active_connections);
	snapshot.gauges["db.connections.idle"] = static_cast<double>(metrics_.idle_connections);
	snapshot.gauges["db.connections.usage_percent"] = metrics_.connection_usage_percent;

	// Query metrics
	snapshot.gauges["db.query.avg_latency_us"] = static_cast<double>(metrics_.avg_query_latency.count());
	snapshot.gauges["db.query.success_rate"] = metrics_.query_success_rate;

	snapshot.counters["db.queries.total"] = metrics_.total_queries;
	snapshot.counters["db.queries.successful"] = metrics_.successful_queries;
	snapshot.counters["db.queries.failed"] = metrics_.failed_queries;

	// Transaction metrics
	snapshot.counters["db.transactions.committed"] = metrics_.committed_transactions;
	snapshot.counters["db.transactions.rolled_back"] = metrics_.rolled_back_transactions;

	// Generic metrics
	for (const auto& [name, value] : generic_metrics_)
	{
		snapshot.gauges[name] = value;
	}

	return snapshot;
}

common::Result<health_check_result> fallback_monitoring_backend::check_health()
{
	if (!initialized_)
	{
		return make_error_result<health_check_result>("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	health_check_result result;
	result.status = health_status::healthy;
	result.message = "Database system healthy";

	// Connection pool health check
	if (metrics_.connection_usage_percent > config_.connection_usage_warning_threshold * 100.0)
	{
		result.status = health_status::degraded;
		result.message = "Connection pool usage critical";
		result.metadata["connection_usage"] = std::to_string(metrics_.connection_usage_percent) + "%";
	}

	// Query latency health check
	if (metrics_.avg_query_latency > config_.query_latency_warning)
	{
		result.status = health_status::degraded;
		result.message = "Query latency critical";
		result.metadata["avg_latency_us"] = std::to_string(metrics_.avg_query_latency.count());
	}

	// Query success rate health check
	if (metrics_.total_queries > 10 && metrics_.query_success_rate < 0.95)
	{
		result.status = health_status::degraded;
		result.message = "Query success rate low";
		result.metadata["success_rate"] = std::to_string(metrics_.query_success_rate);
	}

	return result;
}

common::VoidResult fallback_monitoring_backend::reset()
{
	if (!initialized_)
	{
		return make_error("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	metrics_ = database_metrics{};
	query_latencies_.clear();
	generic_metrics_.clear();
	return common::ok();
}

void fallback_monitoring_backend::record_query_execution(std::chrono::microseconds duration, bool success)
{
	std::lock_guard<std::mutex> lock(mutex_);

	metrics_.total_queries++;
	if (success)
	{
		metrics_.successful_queries++;
	}
	else
	{
		metrics_.failed_queries++;
	}

	if (metrics_.total_queries > 0)
	{
		metrics_.query_success_rate = static_cast<double>(metrics_.successful_queries) / metrics_.total_queries;
	}

	query_latencies_.push_back(duration);

	// Keep last 1000 samples
	const size_t max_samples = 1000;
	if (query_latencies_.size() > max_samples)
	{
		query_latencies_.erase(query_latencies_.begin());
	}

	update_avg_latency();
}

void fallback_monitoring_backend::record_connection_acquired()
{
	// Handled by update_pool_stats
}

void fallback_monitoring_backend::record_connection_released()
{
	// Handled by update_pool_stats
}

void fallback_monitoring_backend::update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
{
	std::lock_guard<std::mutex> lock(mutex_);

	metrics_.active_connections = active;
	metrics_.idle_connections = idle;
	metrics_.total_connections = total;

	if (total > 0)
	{
		metrics_.connection_usage_percent = (static_cast<double>(active) / total) * 100.0;
	}
}

void fallback_monitoring_backend::record_transaction_begin()
{
	std::lock_guard<std::mutex> lock(mutex_);
	metrics_.active_transactions++;
}

void fallback_monitoring_backend::record_transaction_commit()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (metrics_.active_transactions > 0)
	{
		metrics_.active_transactions--;
	}
	metrics_.committed_transactions++;
}

void fallback_monitoring_backend::record_transaction_rollback()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (metrics_.active_transactions > 0)
	{
		metrics_.active_transactions--;
	}
	metrics_.rolled_back_transactions++;
}

common::Result<database_metrics> fallback_monitoring_backend::get_database_metrics()
{
	if (!initialized_)
	{
		return make_error_result<database_metrics>("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	metrics_.timestamp = std::chrono::system_clock::now();
	return metrics_;
}

std::string fallback_monitoring_backend::export_prometheus_metrics()
{
	std::lock_guard<std::mutex> lock(mutex_);

	std::ostringstream oss;
	oss << "# TYPE db_connections_active gauge\n";
	oss << "db_connections_active " << metrics_.active_connections << "\n";

	oss << "# TYPE db_connections_idle gauge\n";
	oss << "db_connections_idle " << metrics_.idle_connections << "\n";

	oss << "# TYPE db_queries_total counter\n";
	oss << "db_queries_total " << metrics_.total_queries << "\n";

	oss << "# TYPE db_queries_success_rate gauge\n";
	oss << "db_queries_success_rate " << metrics_.query_success_rate << "\n";

	oss << "# TYPE db_query_latency_avg_us gauge\n";
	oss << "db_query_latency_avg_us " << metrics_.avg_query_latency.count() << "\n";

	return oss.str();
}

std::chrono::microseconds fallback_monitoring_backend::calculate_percentile(double percentile)
{
	if (query_latencies_.empty())
	{
		return std::chrono::microseconds(0);
	}

	auto sorted = query_latencies_;
	std::sort(sorted.begin(), sorted.end());

	size_t idx = static_cast<size_t>(sorted.size() * percentile);
	if (idx >= sorted.size())
	{
		idx = sorted.size() - 1;
	}

	return sorted[idx];
}

void fallback_monitoring_backend::update_avg_latency()
{
	if (query_latencies_.empty())
	{
		return;
	}

	auto sum = std::accumulate(
		query_latencies_.begin(), query_latencies_.end(), std::chrono::microseconds(0));
	metrics_.avg_query_latency = std::chrono::microseconds(sum.count() / query_latencies_.size());

	metrics_.min_query_latency = *std::min_element(query_latencies_.begin(), query_latencies_.end());
	metrics_.max_query_latency = *std::max_element(query_latencies_.begin(), query_latencies_.end());

	auto sorted = query_latencies_;
	std::sort(sorted.begin(), sorted.end());

	size_t p95_idx = static_cast<size_t>(sorted.size() * 0.95);
	size_t p99_idx = static_cast<size_t>(sorted.size() * 0.99);

	if (p95_idx < sorted.size())
	{
		metrics_.p95_query_latency = sorted[p95_idx];
	}
	if (p99_idx < sorted.size())
	{
		metrics_.p99_query_latency = sorted[p99_idx];
	}
}

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
