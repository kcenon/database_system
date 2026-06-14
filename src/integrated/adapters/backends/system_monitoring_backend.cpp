// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/backends/system_monitoring_backend.h>

#include <kcenon/database/core/result.h>
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/exporters/metric_exporters.h>

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

system_monitoring_backend::system_monitoring_backend(const db_monitoring_config& config)
	: config_(config)
	, initialized_(false)
	, current_metrics_{}
	, max_latency_samples_(10000)
	, active_connections_(0)
	, idle_connections_(0)
	, total_connections_(0)
	, active_transactions_(0)
{
	recent_query_latencies_.reserve(max_latency_samples_);
}

system_monitoring_backend::~system_monitoring_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult system_monitoring_backend::initialize()
{
	if (initialized_)
	{
		return common::ok();
	}

	try
	{
		// Create performance monitor
		monitor_ = std::make_unique<kcenon::monitoring::performance_monitor>("database_system");

		// Initialize the monitor
		auto init_result = monitor_->initialize();
		if (!init_result.is_ok())
		{
			return make_error("Failed to initialize monitoring_system: " +
							  std::string(init_result.error().message));
		}

		start_time_ = std::chrono::steady_clock::now();
		initialized_ = true;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error("Exception during initialization: " + std::string(e.what()));
	}
}

common::VoidResult system_monitoring_backend::shutdown()
{
	if (!initialized_)
	{
		return common::ok();
	}

	try
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (monitor_)
		{
			auto cleanup_result = monitor_->cleanup();
			if (!cleanup_result.is_ok())
			{
				// Log error but continue shutdown
			}
			monitor_.reset();
		}

		current_metrics_ = database_metrics{};
		recent_query_latencies_.clear();
		initialized_ = false;

		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error("Exception during shutdown: " + std::string(e.what()));
	}
}

bool system_monitoring_backend::is_initialized() const
{
	return initialized_;
}

common::VoidResult system_monitoring_backend::record_metric(const std::string& name, double value)
{
	if (!initialized_)
	{
		return make_error("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	try
	{
		// Record as a duration sample for performance profiler
		auto duration_ns = static_cast<std::int64_t>(value * 1000.0); // Assume value is in microseconds
		monitor_->get_profiler().record_sample(
			name,
			std::chrono::nanoseconds(duration_ns),
			true);
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error("Failed to record metric: " + std::string(e.what()));
	}
}

common::VoidResult system_monitoring_backend::record_metric(
	const std::string& name, double value,
	const std::unordered_map<std::string, std::string>& /*tags*/)
{
	// monitoring_system performance_profiler doesn't support tags directly
	// Record without tags for now
	return record_metric(name, value);
}

common::Result<metrics_snapshot> system_monitoring_backend::get_metrics()
{
	if (!initialized_)
	{
		return make_error_result<metrics_snapshot>("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	try
	{
		metrics_snapshot snapshot;
		snapshot.source_id = "database_system";

		// Connection metrics
		snapshot.gauges["db.connections.active"] = static_cast<double>(active_connections_);
		snapshot.gauges["db.connections.idle"] = static_cast<double>(idle_connections_);
		snapshot.gauges["db.connections.total"] = static_cast<double>(total_connections_);
		snapshot.gauges["db.connections.usage_percent"] = current_metrics_.connection_usage_percent;

		// Query metrics
		snapshot.counters["db.queries.total"] = current_metrics_.total_queries;
		snapshot.counters["db.queries.successful"] = current_metrics_.successful_queries;
		snapshot.counters["db.queries.failed"] = current_metrics_.failed_queries;

		snapshot.gauges["db.query.avg_latency_us"] = static_cast<double>(current_metrics_.avg_query_latency.count());
		snapshot.gauges["db.query.min_latency_us"] = static_cast<double>(current_metrics_.min_query_latency.count());
		snapshot.gauges["db.query.max_latency_us"] = static_cast<double>(current_metrics_.max_query_latency.count());
		snapshot.gauges["db.query.p95_latency_us"] = static_cast<double>(current_metrics_.p95_query_latency.count());
		snapshot.gauges["db.query.p99_latency_us"] = static_cast<double>(current_metrics_.p99_query_latency.count());
		snapshot.gauges["db.query.success_rate"] = current_metrics_.query_success_rate;

		// Transaction metrics
		snapshot.gauges["db.transactions.active"] = static_cast<double>(active_transactions_);
		snapshot.counters["db.transactions.committed"] = current_metrics_.committed_transactions;
		snapshot.counters["db.transactions.rolled_back"] = current_metrics_.rolled_back_transactions;
		snapshot.gauges["db.transaction.commit_rate"] = current_metrics_.transaction_commit_rate;

		// Throughput metrics
		snapshot.gauges["db.queries_per_second"] = current_metrics_.queries_per_second;
		snapshot.gauges["db.transactions_per_second"] = current_metrics_.transactions_per_second;

		return snapshot;
	}
	catch (const std::exception& e)
	{
		return make_error_result<metrics_snapshot>("Failed to get metrics: " + std::string(e.what()));
	}
}

common::Result<health_check_result> system_monitoring_backend::check_health()
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
	if (current_metrics_.connection_usage_percent > config_.connection_usage_warning_threshold * 100.0)
	{
		result.status = health_status::degraded;
		result.message = "Connection pool usage critical";
		result.metadata["connection_usage"] =
			std::to_string(current_metrics_.connection_usage_percent) + "%";
	}

	// Query latency health check
	if (current_metrics_.avg_query_latency > config_.query_latency_warning)
	{
		result.status = health_status::degraded;
		result.message = "Query latency critical";
		result.metadata["avg_latency_us"] = std::to_string(current_metrics_.avg_query_latency.count());
	}

	// Query success rate health check
	if (current_metrics_.total_queries > 10 && current_metrics_.query_success_rate < 0.95)
	{
		result.status = health_status::unhealthy;
		result.message = "Query success rate low";
		result.metadata["success_rate"] = std::to_string(current_metrics_.query_success_rate);
	}

	return result;
}

common::VoidResult system_monitoring_backend::reset()
{
	if (!initialized_)
	{
		return make_error("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	try
	{
		current_metrics_ = database_metrics{};
		recent_query_latencies_.clear();
		active_connections_ = 0;
		idle_connections_ = 0;
		total_connections_ = 0;
		active_transactions_ = 0;

		if (monitor_)
		{
			monitor_->reset();
		}

		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error("Failed to reset metrics: " + std::string(e.what()));
	}
}

void system_monitoring_backend::record_query_execution(std::chrono::microseconds duration, bool success)
{
	std::lock_guard<std::mutex> lock(mutex_);

	// Update counters
	current_metrics_.total_queries++;
	if (success)
	{
		current_metrics_.successful_queries++;
	}
	else
	{
		current_metrics_.failed_queries++;
	}

	// Calculate success rate
	if (current_metrics_.total_queries > 0)
	{
		current_metrics_.query_success_rate =
			static_cast<double>(current_metrics_.successful_queries) / current_metrics_.total_queries;
	}

	// Record in monitoring_system
	if (monitor_)
	{
		auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
		monitor_->get_profiler().record_sample("query_execution", duration_ns, success);
	}

	// Store latency sample
	recent_query_latencies_.push_back(duration);
	if (recent_query_latencies_.size() > max_latency_samples_)
	{
		recent_query_latencies_.erase(recent_query_latencies_.begin());
	}

	// Calculate derived metrics
	calculate_derived_metrics();
}

void system_monitoring_backend::record_connection_acquired()
{
	std::lock_guard<std::mutex> lock(mutex_);
	// Connection tracking is handled by update_pool_stats
}

void system_monitoring_backend::record_connection_released()
{
	std::lock_guard<std::mutex> lock(mutex_);
	// Connection tracking is handled by update_pool_stats
}

void system_monitoring_backend::update_pool_stats(std::size_t active, std::size_t idle, std::size_t total)
{
	std::lock_guard<std::mutex> lock(mutex_);

	active_connections_ = active;
	idle_connections_ = idle;
	total_connections_ = total;

	current_metrics_.active_connections = active;
	current_metrics_.idle_connections = idle;
	current_metrics_.total_connections = total;

	// Calculate usage percentage
	if (total > 0)
	{
		current_metrics_.connection_usage_percent = (static_cast<double>(active) / total) * 100.0;
	}
	else
	{
		current_metrics_.connection_usage_percent = 0.0;
	}
}

void system_monitoring_backend::record_transaction_begin()
{
	std::lock_guard<std::mutex> lock(mutex_);
	active_transactions_++;
	current_metrics_.active_transactions = active_transactions_;
}

void system_monitoring_backend::record_transaction_commit()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (active_transactions_ > 0)
	{
		active_transactions_--;
	}

	current_metrics_.active_transactions = active_transactions_;
	current_metrics_.committed_transactions++;

	// Calculate commit rate
	auto total_txns = current_metrics_.committed_transactions + current_metrics_.rolled_back_transactions;
	if (total_txns > 0)
	{
		current_metrics_.transaction_commit_rate =
			static_cast<double>(current_metrics_.committed_transactions) / total_txns;
	}
}

void system_monitoring_backend::record_transaction_rollback()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (active_transactions_ > 0)
	{
		active_transactions_--;
	}

	current_metrics_.active_transactions = active_transactions_;
	current_metrics_.rolled_back_transactions++;

	// Calculate commit rate
	auto total_txns = current_metrics_.committed_transactions + current_metrics_.rolled_back_transactions;
	if (total_txns > 0)
	{
		current_metrics_.transaction_commit_rate =
			static_cast<double>(current_metrics_.committed_transactions) / total_txns;
	}
}

common::Result<database_metrics> system_monitoring_backend::get_database_metrics()
{
	if (!initialized_)
	{
		return make_error_result<database_metrics>("Monitoring backend not initialized");
	}

	std::lock_guard<std::mutex> lock(mutex_);

	// Update timestamp
	current_metrics_.timestamp = std::chrono::system_clock::now();

	// Calculate throughput
	auto elapsed = std::chrono::steady_clock::now() - start_time_;
	auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

	if (elapsed_seconds > 0)
	{
		current_metrics_.queries_per_second =
			static_cast<double>(current_metrics_.total_queries) / elapsed_seconds;
		auto total_txns =
			current_metrics_.committed_transactions + current_metrics_.rolled_back_transactions;
		current_metrics_.transactions_per_second = static_cast<double>(total_txns) / elapsed_seconds;
	}

	return current_metrics_;
}

std::string system_monitoring_backend::export_prometheus_metrics()
{
	std::lock_guard<std::mutex> lock(mutex_);

	std::ostringstream ss;

	// Connection metrics
	ss << "# HELP db_connections_active Number of active database connections\n";
	ss << "# TYPE db_connections_active gauge\n";
	ss << "db_connections_active " << current_metrics_.active_connections << "\n";

	ss << "# HELP db_connections_idle Number of idle database connections\n";
	ss << "# TYPE db_connections_idle gauge\n";
	ss << "db_connections_idle " << current_metrics_.idle_connections << "\n";

	ss << "# HELP db_connections_total Total number of connections in pool\n";
	ss << "# TYPE db_connections_total gauge\n";
	ss << "db_connections_total " << current_metrics_.total_connections << "\n";

	ss << "# HELP db_connection_usage_percent Connection pool usage percentage\n";
	ss << "# TYPE db_connection_usage_percent gauge\n";
	ss << "db_connection_usage_percent " << current_metrics_.connection_usage_percent << "\n";

	// Query metrics
	ss << "# HELP db_queries_total Total number of queries executed\n";
	ss << "# TYPE db_queries_total counter\n";
	ss << "db_queries_total " << current_metrics_.total_queries << "\n";

	ss << "# HELP db_queries_successful Number of successful queries\n";
	ss << "# TYPE db_queries_successful counter\n";
	ss << "db_queries_successful " << current_metrics_.successful_queries << "\n";

	ss << "# HELP db_queries_failed Number of failed queries\n";
	ss << "# TYPE db_queries_failed counter\n";
	ss << "db_queries_failed " << current_metrics_.failed_queries << "\n";

	ss << "# HELP db_query_success_rate Query success rate (0-1)\n";
	ss << "# TYPE db_query_success_rate gauge\n";
	ss << "db_query_success_rate " << current_metrics_.query_success_rate << "\n";

	// Latency metrics
	ss << "# HELP db_query_latency_avg_us Average query latency in microseconds\n";
	ss << "# TYPE db_query_latency_avg_us gauge\n";
	ss << "db_query_latency_avg_us " << current_metrics_.avg_query_latency.count() << "\n";

	ss << "# HELP db_query_latency_min_us Minimum query latency in microseconds\n";
	ss << "# TYPE db_query_latency_min_us gauge\n";
	ss << "db_query_latency_min_us " << current_metrics_.min_query_latency.count() << "\n";

	ss << "# HELP db_query_latency_max_us Maximum query latency in microseconds\n";
	ss << "# TYPE db_query_latency_max_us gauge\n";
	ss << "db_query_latency_max_us " << current_metrics_.max_query_latency.count() << "\n";

	ss << "# HELP db_query_latency_p95_us 95th percentile query latency in microseconds\n";
	ss << "# TYPE db_query_latency_p95_us gauge\n";
	ss << "db_query_latency_p95_us " << current_metrics_.p95_query_latency.count() << "\n";

	ss << "# HELP db_query_latency_p99_us 99th percentile query latency in microseconds\n";
	ss << "# TYPE db_query_latency_p99_us gauge\n";
	ss << "db_query_latency_p99_us " << current_metrics_.p99_query_latency.count() << "\n";

	// Transaction metrics
	ss << "# HELP db_transactions_active Number of active transactions\n";
	ss << "# TYPE db_transactions_active gauge\n";
	ss << "db_transactions_active " << current_metrics_.active_transactions << "\n";

	ss << "# HELP db_transactions_committed Total number of committed transactions\n";
	ss << "# TYPE db_transactions_committed counter\n";
	ss << "db_transactions_committed " << current_metrics_.committed_transactions << "\n";

	ss << "# HELP db_transactions_rolled_back Total number of rolled back transactions\n";
	ss << "# TYPE db_transactions_rolled_back counter\n";
	ss << "db_transactions_rolled_back " << current_metrics_.rolled_back_transactions << "\n";

	ss << "# HELP db_transaction_commit_rate Transaction commit rate (0-1)\n";
	ss << "# TYPE db_transaction_commit_rate gauge\n";
	ss << "db_transaction_commit_rate " << current_metrics_.transaction_commit_rate << "\n";

	// Throughput metrics
	ss << "# HELP db_queries_per_second Query throughput (queries/second)\n";
	ss << "# TYPE db_queries_per_second gauge\n";
	ss << "db_queries_per_second " << current_metrics_.queries_per_second << "\n";

	ss << "# HELP db_transactions_per_second Transaction throughput (transactions/second)\n";
	ss << "# TYPE db_transactions_per_second gauge\n";
	ss << "db_transactions_per_second " << current_metrics_.transactions_per_second << "\n";

	return ss.str();
}

void system_monitoring_backend::calculate_derived_metrics()
{
	if (recent_query_latencies_.empty())
	{
		return;
	}

	// Create sorted copy for percentile calculation
	auto sorted_latencies = recent_query_latencies_;
	std::sort(sorted_latencies.begin(), sorted_latencies.end());

	// Calculate min/max
	current_metrics_.min_query_latency = sorted_latencies.front();
	current_metrics_.max_query_latency = sorted_latencies.back();

	// Calculate average
	auto total_latency = std::accumulate(
		sorted_latencies.begin(),
		sorted_latencies.end(),
		std::chrono::microseconds(0));
	current_metrics_.avg_query_latency = total_latency / sorted_latencies.size();

	// Calculate percentiles
	auto calc_percentile = [&sorted_latencies](double percentile) -> std::chrono::microseconds {
		if (sorted_latencies.empty())
			return std::chrono::microseconds(0);

		std::size_t index =
			static_cast<std::size_t>((percentile / 100.0) * (sorted_latencies.size() - 1));
		return sorted_latencies[index];
	};

	current_metrics_.p95_query_latency = calc_percentile(95.0);
	current_metrics_.p99_query_latency = calc_percentile(99.0);
}

database_metrics system_monitoring_backend::convert_to_database_metrics(
	const kcenon::monitoring::metrics_snapshot& snapshot)
{
	database_metrics db_metrics;

	// Extract metrics from snapshot
	for (const auto& metric : snapshot.metrics)
	{
		const auto& name = metric.name;
		const auto value = metric.value;

		if (name == "query_execution_count")
		{
			db_metrics.total_queries = static_cast<std::uint64_t>(value);
		}
		// Add more metric mappings as needed
	}

	return db_metrics;
}

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
