// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "monitoring_adapter.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <sstream>

// Conditional includes based on monitoring_system availability
#if defined(USE_MONITORING_SYSTEM)
	#include <kcenon/monitoring/core/performance_monitor.h>
	#include <kcenon/monitoring/core/system_monitor.h>
#else
	#include <atomic>
	#include <mutex>
	#include <vector>
#endif

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════

namespace
{

inline common::VoidResult make_error(const std::string& msg, int code = -1)
{
#if defined(USE_COMMON_SYSTEM)
	return common::VoidResult(common::error_info{ code, msg });
#else
	return common::VoidResult(common::Error{ msg, code });
#endif
}

template <typename T>
inline common::Result<T> make_error_result(const std::string& msg, int code = -1)
{
#if defined(USE_COMMON_SYSTEM)
	return common::Result<T>(common::error_info{ code, msg });
#else
	return common::Result<T>(common::Error{ msg, code });
#endif
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// PIMPL Implementation - WITH monitoring_system
// ═══════════════════════════════════════════════════════════════

#if defined(USE_MONITORING_SYSTEM)

class monitoring_adapter::impl
{
public:
	explicit impl(const db_monitoring_config& config)
		: config_(config), initialized_(false)
	{
	}

	~impl()
	{
		if (initialized_)
		{
			shutdown();
		}
	}

	common::VoidResult initialize()
	{
		if (initialized_)
		{
			return common::ok();
		}

		try
		{
			// Create monitoring_system's performance profiler
			profiler_ = std::make_unique<monitoring_system::performance_profiler>();

			if (config_.enable_profiling)
			{
				profiler_->set_enabled(true);
			}

			// Create system monitor if health checks enabled
			if (config_.enable_health_checks)
			{
				system_monitor_ = std::make_unique<monitoring_system::system_monitor>();
			}

			start_time_ = std::chrono::steady_clock::now();
			initialized_ = true;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Monitoring adapter initialization failed: ")
				+ e.what());
		}
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok();
		}

		try
		{
			if (profiler_)
			{
				profiler_->clear_all_samples();
				profiler_.reset();
			}
			system_monitor_.reset();
			initialized_ = false;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Monitoring shutdown failed: ") + e.what());
		}
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	common::VoidResult record_metric(const std::string& name, double value)
	{
		if (!initialized_)
		{
			return make_error("Monitoring adapter not initialized");
		}

		if (profiler_)
		{
			auto duration = std::chrono::nanoseconds(static_cast<int64_t>(value));
			auto result = profiler_->record_sample(name, duration, true);
			if (!result)
			{
				return make_error("Failed to record metric");
			}
		}
		return common::ok();
	}

	common::VoidResult record_metric(const std::string& name, double value,
		const std::unordered_map<std::string, std::string>& /*tags*/)
	{
		// Tags not currently supported - forward to basic record
		return record_metric(name, value);
	}

	common::Result<common::interfaces::metrics_snapshot> get_metrics()
	{
		if (!initialized_)
		{
			return make_error_result<common::interfaces::metrics_snapshot>(
				"Monitoring adapter not initialized");
		}

		common::interfaces::metrics_snapshot snapshot;

		// Add database metrics to snapshot
		snapshot.gauges["db.connections.active"] = static_cast<double>(metrics_.active_connections);
		snapshot.gauges["db.connections.idle"] = static_cast<double>(metrics_.idle_connections);
		snapshot.gauges["db.connections.usage_percent"] = metrics_.connection_usage_percent;
		snapshot.gauges["db.query.avg_latency_us"]
			= static_cast<double>(metrics_.avg_query_latency.count());
		snapshot.gauges["db.query.success_rate"] = metrics_.query_success_rate;

		snapshot.counters["db.queries.total"] = metrics_.total_queries;
		snapshot.counters["db.queries.successful"] = metrics_.successful_queries;
		snapshot.counters["db.queries.failed"] = metrics_.failed_queries;
		snapshot.counters["db.transactions.committed"] = metrics_.committed_transactions;
		snapshot.counters["db.transactions.rolled_back"] = metrics_.rolled_back_transactions;

		return common::Result<common::interfaces::metrics_snapshot>(snapshot);
	}

	common::Result<common::interfaces::health_check_result> check_health()
	{
		if (!initialized_)
		{
			return make_error_result<common::interfaces::health_check_result>(
				"Monitoring adapter not initialized");
		}

		common::interfaces::health_check_result result;
		result.is_healthy = true;
		result.status_message = "Database system healthy";

		// Check connection pool usage
		if (metrics_.connection_usage_percent
			> config_.connection_usage_warning_threshold * 100.0)
		{
			result.is_healthy = false;
			result.status_message = "Connection pool usage critical";
			result.details["connection_usage"]
				= std::to_string(metrics_.connection_usage_percent) + "%";
		}

		// Check query latency
		if (metrics_.avg_query_latency > config_.query_latency_warning)
		{
			result.is_healthy = false;
			result.status_message = "Query latency critical";
			result.details["avg_latency_us"]
				= std::to_string(metrics_.avg_query_latency.count());
		}

		// Check query success rate
		if (metrics_.query_success_rate < 0.95)
		{
			result.is_healthy = false;
			result.status_message = "Query success rate low";
			result.details["success_rate"] = std::to_string(metrics_.query_success_rate);
		}

		return common::Result<common::interfaces::health_check_result>(result);
	}

	common::VoidResult reset()
	{
		if (profiler_)
		{
			profiler_->clear_all_samples();
		}

		metrics_ = database_metrics{};
		query_latencies_.clear();
		return common::ok();
	}

	void record_query_execution(std::chrono::microseconds duration, bool success)
	{
		metrics_.total_queries++;
		if (success)
		{
			metrics_.successful_queries++;
		}
		else
		{
			metrics_.failed_queries++;
		}

		// Update success rate
		if (metrics_.total_queries > 0)
		{
			metrics_.query_success_rate
				= static_cast<double>(metrics_.successful_queries) / metrics_.total_queries;
		}

		// Track latency
		query_latencies_.push_back(duration);

		// Keep only last N samples for percentile calculation
		const size_t max_samples = 1000;
		if (query_latencies_.size() > max_samples)
		{
			query_latencies_.erase(query_latencies_.begin());
		}

		// Update latency metrics
		update_latency_stats();

		// Record to profiler if available
		if (profiler_)
		{
			profiler_->record_sample(
				"db.query", std::chrono::duration_cast<std::chrono::nanoseconds>(duration), success);
		}
	}

	void record_connection_acquired()
	{
		// Handled by update_pool_stats
	}

	void record_connection_released()
	{
		// Handled by update_pool_stats
	}

	void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
	{
		metrics_.active_connections = active;
		metrics_.idle_connections = idle;
		metrics_.total_connections = total;

		if (total > 0)
		{
			metrics_.connection_usage_percent = (static_cast<double>(active) / total) * 100.0;
		}
	}

	void record_transaction_begin()
	{
		metrics_.active_transactions++;
	}

	void record_transaction_commit()
	{
		if (metrics_.active_transactions > 0)
		{
			metrics_.active_transactions--;
		}
		metrics_.committed_transactions++;
	}

	void record_transaction_rollback()
	{
		if (metrics_.active_transactions > 0)
		{
			metrics_.active_transactions--;
		}
		metrics_.rolled_back_transactions++;
	}

	common::Result<database_metrics> get_database_metrics()
	{
		if (!initialized_)
		{
			return make_error_result<database_metrics>("Monitoring adapter not initialized");
		}

		metrics_.timestamp = std::chrono::system_clock::now();
		return common::Result<database_metrics>(metrics_);
	}

	std::string export_prometheus_metrics()
	{
		std::ostringstream oss;

		// Connection pool metrics
		oss << "# HELP db_connections_active Active database connections\n";
		oss << "# TYPE db_connections_active gauge\n";
		oss << "db_connections_active " << metrics_.active_connections << "\n\n";

		oss << "# HELP db_connections_idle Idle database connections\n";
		oss << "# TYPE db_connections_idle gauge\n";
		oss << "db_connections_idle " << metrics_.idle_connections << "\n\n";

		// Query metrics
		oss << "# HELP db_queries_total Total number of queries\n";
		oss << "# TYPE db_queries_total counter\n";
		oss << "db_queries_total " << metrics_.total_queries << "\n\n";

		oss << "# HELP db_queries_successful Successfully executed queries\n";
		oss << "# TYPE db_queries_successful counter\n";
		oss << "db_queries_successful " << metrics_.successful_queries << "\n\n";

		oss << "# HELP db_query_latency_microseconds Query execution latency\n";
		oss << "# TYPE db_query_latency_microseconds summary\n";
		oss << "db_query_latency_microseconds{quantile=\"0.95\"} "
			<< metrics_.p95_query_latency.count() << "\n";
		oss << "db_query_latency_microseconds{quantile=\"0.99\"} "
			<< metrics_.p99_query_latency.count() << "\n";
		oss << "db_query_latency_microseconds_sum "
			<< metrics_.avg_query_latency.count() * metrics_.total_queries << "\n";
		oss << "db_query_latency_microseconds_count " << metrics_.total_queries << "\n\n";

		// Transaction metrics
		oss << "# HELP db_transactions_committed Total committed transactions\n";
		oss << "# TYPE db_transactions_committed counter\n";
		oss << "db_transactions_committed " << metrics_.committed_transactions << "\n\n";

		oss << "# HELP db_transactions_rolled_back Total rolled back transactions\n";
		oss << "# TYPE db_transactions_rolled_back counter\n";
		oss << "db_transactions_rolled_back " << metrics_.rolled_back_transactions << "\n\n";

		return oss.str();
	}

private:
	void update_latency_stats()
	{
		if (query_latencies_.empty())
		{
			return;
		}

		// Calculate average
		auto sum = std::accumulate(
			query_latencies_.begin(), query_latencies_.end(), std::chrono::microseconds(0));
		metrics_.avg_query_latency
			= std::chrono::microseconds(sum.count() / query_latencies_.size());

		// Calculate min/max
		metrics_.min_query_latency = *std::min_element(query_latencies_.begin(), query_latencies_.end());
		metrics_.max_query_latency = *std::max_element(query_latencies_.begin(), query_latencies_.end());

		// Calculate percentiles
		auto sorted_latencies = query_latencies_;
		std::sort(sorted_latencies.begin(), sorted_latencies.end());

		size_t p95_idx = static_cast<size_t>(sorted_latencies.size() * 0.95);
		size_t p99_idx = static_cast<size_t>(sorted_latencies.size() * 0.99);

		if (p95_idx < sorted_latencies.size())
		{
			metrics_.p95_query_latency = sorted_latencies[p95_idx];
		}

		if (p99_idx < sorted_latencies.size())
		{
			metrics_.p99_query_latency = sorted_latencies[p99_idx];
		}
	}

	const db_monitoring_config& config_;
	bool initialized_;
	std::unique_ptr<monitoring_system::performance_profiler> profiler_;
	std::unique_ptr<monitoring_system::system_monitor> system_monitor_;
	std::chrono::steady_clock::time_point start_time_;

	database_metrics metrics_;
	std::vector<std::chrono::microseconds> query_latencies_;
};

#else

// ═══════════════════════════════════════════════════════════════
// PIMPL Implementation - Fallback (internal metrics)
// ═══════════════════════════════════════════════════════════════

class monitoring_adapter::impl
{
public:
	explicit impl(const db_monitoring_config& config)
		: config_(config), initialized_(false)
	{
	}

	~impl()
	{
		if (initialized_)
		{
			shutdown();
		}
	}

	common::VoidResult initialize()
	{
		if (initialized_)
		{
			return common::ok();
		}

		start_time_ = std::chrono::steady_clock::now();
		initialized_ = true;
		return common::ok();
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok();
		}

		std::lock_guard<std::mutex> lock(mutex_);
		metrics_ = database_metrics{};
		query_latencies_.clear();
		initialized_ = false;
		return common::ok();
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	common::VoidResult record_metric(const std::string& name, double value)
	{
		if (!initialized_)
		{
			return make_error("Monitoring adapter not initialized");
		}

		std::lock_guard<std::mutex> lock(mutex_);
		generic_metrics_[name] = value;
		return common::ok();
	}

	common::VoidResult record_metric(const std::string& name, double value,
		const std::unordered_map<std::string, std::string>& /*tags*/)
	{
		return record_metric(name, value);
	}

	common::Result<common::interfaces::metrics_snapshot> get_metrics()
	{
		if (!initialized_)
		{
			return make_error_result<common::interfaces::metrics_snapshot>(
				"Monitoring adapter not initialized");
		}

		std::lock_guard<std::mutex> lock(mutex_);

		common::interfaces::metrics_snapshot snapshot;

		// Database-specific metrics as gauges
		snapshot.gauges["db.connections.active"] = static_cast<double>(metrics_.active_connections);
		snapshot.gauges["db.connections.idle"] = static_cast<double>(metrics_.idle_connections);
		snapshot.gauges["db.connections.usage_percent"] = metrics_.connection_usage_percent;
		snapshot.gauges["db.query.avg_latency_us"]
			= static_cast<double>(metrics_.avg_query_latency.count());
		snapshot.gauges["db.query.success_rate"] = metrics_.query_success_rate;

		// Counters
		snapshot.counters["db.queries.total"] = metrics_.total_queries;
		snapshot.counters["db.queries.successful"] = metrics_.successful_queries;
		snapshot.counters["db.queries.failed"] = metrics_.failed_queries;
		snapshot.counters["db.transactions.committed"] = metrics_.committed_transactions;
		snapshot.counters["db.transactions.rolled_back"] = metrics_.rolled_back_transactions;

		// Generic metrics
		for (const auto& [name, value] : generic_metrics_)
		{
			snapshot.gauges[name] = value;
		}

		return common::Result<common::interfaces::metrics_snapshot>(snapshot);
	}

	common::Result<common::interfaces::health_check_result> check_health()
	{
		if (!initialized_)
		{
			return make_error_result<common::interfaces::health_check_result>(
				"Monitoring adapter not initialized");
		}

		std::lock_guard<std::mutex> lock(mutex_);

		common::interfaces::health_check_result result;
		result.is_healthy = true;
		result.status_message = "Database system healthy";

		// Connection pool health check
		if (metrics_.connection_usage_percent
			> config_.connection_usage_warning_threshold * 100.0)
		{
			result.is_healthy = false;
			result.status_message = "Connection pool usage critical";
		}

		// Query latency health check
		if (metrics_.avg_query_latency > config_.query_latency_warning)
		{
			result.is_healthy = false;
			result.status_message = "Query latency critical";
		}

		// Query success rate health check
		if (metrics_.total_queries > 10 && metrics_.query_success_rate < 0.95)
		{
			result.is_healthy = false;
			result.status_message = "Query success rate low";
		}

		return common::Result<common::interfaces::health_check_result>(result);
	}

	common::VoidResult reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		metrics_ = database_metrics{};
		query_latencies_.clear();
		generic_metrics_.clear();
		return common::ok();
	}

	void record_query_execution(std::chrono::microseconds duration, bool success)
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
			metrics_.query_success_rate
				= static_cast<double>(metrics_.successful_queries) / metrics_.total_queries;
		}

		query_latencies_.push_back(duration);

		// Keep last 1000 samples
		const size_t max_samples = 1000;
		if (query_latencies_.size() > max_samples)
		{
			query_latencies_.erase(query_latencies_.begin());
		}

		update_latency_stats();
	}

	void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
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

	void record_transaction_begin()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		metrics_.active_transactions++;
	}

	void record_transaction_commit()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (metrics_.active_transactions > 0)
		{
			metrics_.active_transactions--;
		}
		metrics_.committed_transactions++;
	}

	void record_transaction_rollback()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (metrics_.active_transactions > 0)
		{
			metrics_.active_transactions--;
		}
		metrics_.rolled_back_transactions++;
	}

	common::Result<database_metrics> get_database_metrics()
	{
		if (!initialized_)
		{
			return make_error_result<database_metrics>("Monitoring adapter not initialized");
		}

		std::lock_guard<std::mutex> lock(mutex_);
		metrics_.timestamp = std::chrono::system_clock::now();
		return common::Result<database_metrics>(metrics_);
	}

	std::string export_prometheus_metrics()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		std::ostringstream oss;

		oss << "# HELP db_connections_active Active database connections\n";
		oss << "# TYPE db_connections_active gauge\n";
		oss << "db_connections_active " << metrics_.active_connections << "\n\n";

		oss << "# HELP db_queries_total Total number of queries\n";
		oss << "# TYPE db_queries_total counter\n";
		oss << "db_queries_total " << metrics_.total_queries << "\n\n";

		return oss.str();
	}

private:
	void update_latency_stats()
	{
		if (query_latencies_.empty())
		{
			return;
		}

		auto sum = std::accumulate(
			query_latencies_.begin(), query_latencies_.end(), std::chrono::microseconds(0));
		metrics_.avg_query_latency
			= std::chrono::microseconds(sum.count() / query_latencies_.size());

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

	const db_monitoring_config& config_;
	bool initialized_;
	std::chrono::steady_clock::time_point start_time_;

	std::mutex mutex_;
	database_metrics metrics_;
	std::vector<std::chrono::microseconds> query_latencies_;
	std::unordered_map<std::string, double> generic_metrics_;
};

#endif

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

monitoring_adapter::monitoring_adapter(const db_monitoring_config& config)
	: pimpl_(std::make_unique<impl>(config))
{
}

monitoring_adapter::~monitoring_adapter() = default;

monitoring_adapter::monitoring_adapter(monitoring_adapter&&) noexcept = default;
monitoring_adapter& monitoring_adapter::operator=(monitoring_adapter&&) noexcept = default;

common::VoidResult monitoring_adapter::initialize()
{
	return pimpl_->initialize();
}

common::VoidResult monitoring_adapter::shutdown()
{
	return pimpl_->shutdown();
}

bool monitoring_adapter::is_initialized() const
{
	return pimpl_->is_initialized();
}

common::VoidResult monitoring_adapter::record_metric(const std::string& name, double value)
{
	return pimpl_->record_metric(name, value);
}

common::VoidResult monitoring_adapter::record_metric(
	const std::string& name, double value,
	const std::unordered_map<std::string, std::string>& tags)
{
	return pimpl_->record_metric(name, value, tags);
}

common::Result<common::interfaces::metrics_snapshot> monitoring_adapter::get_metrics()
{
	return pimpl_->get_metrics();
}

common::Result<common::interfaces::health_check_result> monitoring_adapter::check_health()
{
	return pimpl_->check_health();
}

common::VoidResult monitoring_adapter::reset()
{
	return pimpl_->reset();
}

void monitoring_adapter::record_query_execution(std::chrono::microseconds duration, bool success)
{
	pimpl_->record_query_execution(duration, success);
}

void monitoring_adapter::record_connection_acquired()
{
	pimpl_->record_connection_acquired();
}

void monitoring_adapter::record_connection_released()
{
	pimpl_->record_connection_released();
}

void monitoring_adapter::update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
{
	pimpl_->update_pool_stats(active, idle, total);
}

void monitoring_adapter::record_transaction_begin()
{
	pimpl_->record_transaction_begin();
}

void monitoring_adapter::record_transaction_commit()
{
	pimpl_->record_transaction_commit();
}

void monitoring_adapter::record_transaction_rollback()
{
	pimpl_->record_transaction_rollback();
}

common::Result<database_metrics> monitoring_adapter::get_database_metrics()
{
	return pimpl_->get_database_metrics();
}

std::string monitoring_adapter::export_prometheus_metrics()
{
	return pimpl_->export_prometheus_metrics();
}

} // namespace adapters
} // namespace integrated
} // namespace database
