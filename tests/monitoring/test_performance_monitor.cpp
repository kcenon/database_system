// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Unit tests for performance_monitor and pool_metrics gap coverage.
 * Part of #367, sub-issue #379.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

#include <kcenon/database/monitoring/performance_monitor.h>
#include <kcenon/database/monitoring/pool_metrics.h>

using namespace kcenon::database;
using namespace kcenon::database::monitoring;

//=============================================================================
// Helper: create a query_metrics with given parameters
//=============================================================================

static query_metrics make_query(
	const std::string& hash,
	std::chrono::microseconds exec_time,
	bool success,
	database_types db_type = database_types::postgres,
	const std::string& error_msg = "")
{
	query_metrics m;
	m.query_hash = hash;
	m.start_time = std::chrono::steady_clock::now();
	m.end_time = m.start_time + exec_time;
	m.execution_time = exec_time;
	m.success = success;
	m.error_message = error_msg;
	m.db_type = db_type;
	m.rows_affected = success ? 1 : 0;
	return m;
}

//=============================================================================
// performance_monitor: record_query_metrics Tests
//=============================================================================

class PerformanceMonitorQueryTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(PerformanceMonitorQueryTest, RecordAndRetrieveSingleQuery) {
	auto m = make_query("hash1", std::chrono::microseconds(5000), true);
	monitor_->record_query_metrics(m);

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 1u);
	EXPECT_EQ(summary.successful_queries, 1u);
	EXPECT_EQ(summary.failed_queries, 0u);
}

TEST_F(PerformanceMonitorQueryTest, RecordMultipleQueries) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(1000), true));
	monitor_->record_query_metrics(
		make_query("q2", std::chrono::microseconds(2000), true));
	monitor_->record_query_metrics(
		make_query("q3", std::chrono::microseconds(3000), false,
			database_types::postgres, "timeout"));

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 3u);
	EXPECT_EQ(summary.successful_queries, 2u);
	EXPECT_EQ(summary.failed_queries, 1u);
	EXPECT_GT(summary.error_rate, 0.0);
}

TEST_F(PerformanceMonitorQueryTest, AvgQueryTimeCalculation) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(1000), true));
	monitor_->record_query_metrics(
		make_query("q2", std::chrono::microseconds(3000), true));

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.avg_query_time.count(), 2000);
}

//=============================================================================
// performance_monitor: monitoring_enabled Tests
//=============================================================================

class MonitoringEnabledTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(MonitoringEnabledTest, DisabledDoesNotRecordQueryMetrics) {
	monitor_->set_monitoring_enabled(false);
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(1000), true));

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 0u);
}

TEST_F(MonitoringEnabledTest, DisabledDoesNotRecordConnectionMetrics) {
	monitor_->set_monitoring_enabled(false);

	connection_metrics cm;
	cm.total_connections.store(10);
	cm.active_connections.store(5);
	monitor_->record_connection_metrics(database_types::postgres, cm);

	auto retrieved = monitor_->get_connection_metrics(database_types::postgres);
	EXPECT_EQ(retrieved.total_connections.load(), 0u);
}

TEST_F(MonitoringEnabledTest, DisabledDoesNotUpdateConnectionCount) {
	monitor_->set_monitoring_enabled(false);
	monitor_->update_connection_count(database_types::sqlite, 5, 10);

	auto retrieved = monitor_->get_connection_metrics(database_types::postgres);
	EXPECT_EQ(retrieved.total_connections.load(), 0u);
}

TEST_F(MonitoringEnabledTest, ReEnableResumesRecording) {
	monitor_->set_monitoring_enabled(false);
	monitor_->record_query_metrics(
		make_query("ignored", std::chrono::microseconds(100), true));

	monitor_->set_monitoring_enabled(true);
	monitor_->record_query_metrics(
		make_query("recorded", std::chrono::microseconds(200), true));

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 1u);
}

//=============================================================================
// performance_monitor: get_performance_summary(db_type) Tests
//=============================================================================

class FilteredSummaryTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
		// Record queries for different database types
		monitor_->record_query_metrics(
			make_query("pg1", std::chrono::microseconds(1000), true, database_types::postgres));
		monitor_->record_query_metrics(
			make_query("pg2", std::chrono::microseconds(2000), true, database_types::postgres));
		monitor_->record_query_metrics(
			make_query("sq1", std::chrono::microseconds(3000), true, database_types::sqlite));
		monitor_->record_query_metrics(
			make_query("sq2", std::chrono::microseconds(4000), false,
				database_types::sqlite, "connection lost"));
	}
};

TEST_F(FilteredSummaryTest, PostgresSummaryFiltersCorrectly) {
	auto summary = monitor_->get_performance_summary(database_types::postgres);
	EXPECT_EQ(summary.total_queries, 2u);
	EXPECT_EQ(summary.successful_queries, 2u);
	EXPECT_EQ(summary.failed_queries, 0u);
}

TEST_F(FilteredSummaryTest, SqliteSummaryFiltersCorrectly) {
	auto summary = monitor_->get_performance_summary(database_types::sqlite);
	EXPECT_EQ(summary.total_queries, 2u);
	EXPECT_EQ(summary.successful_queries, 1u);
	EXPECT_EQ(summary.failed_queries, 1u);
	EXPECT_DOUBLE_EQ(summary.error_rate, 0.5);
}

TEST_F(FilteredSummaryTest, UnusedDbTypeReturnsZero) {
	auto summary = monitor_->get_performance_summary(database_types::mongodb);
	EXPECT_EQ(summary.total_queries, 0u);
}

TEST_F(FilteredSummaryTest, OverallSummaryIncludesAll) {
	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 4u);
}

//=============================================================================
// performance_monitor: get_recent_queries / get_slow_queries Tests
//=============================================================================

class QueryHistoryTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(QueryHistoryTest, GetRecentQueriesWithinWindow) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(100), true));
	monitor_->record_query_metrics(
		make_query("q2", std::chrono::microseconds(200), true));

	auto recent = monitor_->get_recent_queries(std::chrono::minutes(5));
	EXPECT_EQ(recent.size(), 2u);
}

TEST_F(QueryHistoryTest, GetRecentQueriesEmptyWindow) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(100), true));

	// Use zero-length window — queries just recorded should still be within
	// the window since their start_time is "now"
	auto recent = monitor_->get_recent_queries(std::chrono::minutes(0));
	// With 0-minute window, cutoff == now, so queries at exactly now are borderline
	// Result depends on timing; we just verify no crash
	EXPECT_LE(recent.size(), 1u);
}

TEST_F(QueryHistoryTest, GetSlowQueriesAboveThreshold) {
	monitor_->record_query_metrics(
		make_query("fast", std::chrono::microseconds(100), true));
	monitor_->record_query_metrics(
		make_query("medium", std::chrono::microseconds(5000), true));
	monitor_->record_query_metrics(
		make_query("slow", std::chrono::microseconds(50000), true));

	auto slow = monitor_->get_slow_queries(std::chrono::microseconds(4000));
	EXPECT_EQ(slow.size(), 2u);
}

TEST_F(QueryHistoryTest, GetSlowQueriesNoneAboveThreshold) {
	monitor_->record_query_metrics(
		make_query("fast", std::chrono::microseconds(100), true));

	auto slow = monitor_->get_slow_queries(std::chrono::microseconds(1000000));
	EXPECT_TRUE(slow.empty());
}

TEST_F(QueryHistoryTest, GetSlowQueriesPreservesMetadata) {
	auto original = make_query("slow_hash", std::chrono::microseconds(99999), false,
		database_types::sqlite, "timeout");
	monitor_->record_query_metrics(original);

	auto slow = monitor_->get_slow_queries(std::chrono::microseconds(1000));
	ASSERT_EQ(slow.size(), 1u);
	EXPECT_EQ(slow[0].query_hash, "slow_hash");
	EXPECT_FALSE(slow[0].success);
	EXPECT_EQ(slow[0].error_message, "timeout");
	EXPECT_EQ(slow[0].db_type, database_types::sqlite);
}

//=============================================================================
// performance_monitor: Alert System Tests
//=============================================================================

class AlertSystemTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(AlertSystemTest, SlowQueryTriggersAlert) {
	// Set low latency threshold so our query triggers an alert
	monitor_->set_alert_thresholds(0.05, std::chrono::microseconds(100));

	monitor_->record_query_metrics(
		make_query("slow", std::chrono::microseconds(500), true));

	auto alerts = monitor_->get_recent_alerts(std::chrono::minutes(5));
	ASSERT_GE(alerts.size(), 1u);

	bool found_slow_query_alert = false;
	for (const auto& alert : alerts) {
		if (alert.type() == performance_alert::alert_type::slow_query) {
			found_slow_query_alert = true;
			EXPECT_FALSE(alert.message().empty());
		}
	}
	EXPECT_TRUE(found_slow_query_alert);
}

TEST_F(AlertSystemTest, RegisterAlertHandlerReceivesCallback) {
	std::atomic<int> callback_count{0};
	monitor_->register_alert_handler([&callback_count](const performance_alert&) {
		callback_count.fetch_add(1);
	});

	// Trigger alert via slow query
	monitor_->set_alert_thresholds(0.05, std::chrono::microseconds(10));
	monitor_->record_query_metrics(
		make_query("slow", std::chrono::microseconds(1000), true));

	EXPECT_GE(callback_count.load(), 1);
}

TEST_F(AlertSystemTest, MultipleHandlersAllCalled) {
	std::atomic<int> handler1_count{0};
	std::atomic<int> handler2_count{0};

	monitor_->register_alert_handler([&handler1_count](const performance_alert&) {
		handler1_count.fetch_add(1);
	});
	monitor_->register_alert_handler([&handler2_count](const performance_alert&) {
		handler2_count.fetch_add(1);
	});

	monitor_->set_alert_thresholds(0.05, std::chrono::microseconds(10));
	monitor_->record_query_metrics(
		make_query("slow", std::chrono::microseconds(1000), true));

	EXPECT_GE(handler1_count.load(), 1);
	EXPECT_GE(handler2_count.load(), 1);
}

TEST_F(AlertSystemTest, GetRecentAlertsEmptyByDefault) {
	auto alerts = monitor_->get_recent_alerts(std::chrono::minutes(5));
	EXPECT_TRUE(alerts.empty());
}

TEST_F(AlertSystemTest, RecordSlowQueryEmitsAlert) {
	monitor_->record_slow_query("SELECT * FROM huge_table",
		std::chrono::microseconds(5000000));

	auto alerts = monitor_->get_recent_alerts(std::chrono::minutes(5));
	ASSERT_GE(alerts.size(), 1u);
	EXPECT_EQ(alerts.back().type(), performance_alert::alert_type::slow_query);
}

TEST_F(AlertSystemTest, ConnectionPoolExhaustionAlert) {
	connection_metrics cm;
	cm.total_connections.store(10);
	cm.active_connections.store(10); // 100% utilization > 90% threshold

	monitor_->record_connection_metrics(database_types::postgres, cm);

	auto alerts = monitor_->get_recent_alerts(std::chrono::minutes(5));
	bool found_pool_alert = false;
	for (const auto& alert : alerts) {
		if (alert.type() == performance_alert::alert_type::connection_pool_exhaustion) {
			found_pool_alert = true;
		}
	}
	EXPECT_TRUE(found_pool_alert);
}

//=============================================================================
// performance_monitor: Metric Lifecycle Tests
//=============================================================================

class MetricLifecycleTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(MetricLifecycleTest, ClearMetricsRemovesEverything) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(100), true));
	monitor_->update_connection_count(database_types::postgres, 5, 10);
	monitor_->record_slow_query("SELECT 1", std::chrono::microseconds(999999));

	monitor_->clear_metrics();

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 0u);
	EXPECT_EQ(summary.total_connections, 0u);

	auto alerts = monitor_->get_recent_alerts(std::chrono::minutes(60));
	EXPECT_TRUE(alerts.empty());
}

TEST_F(MetricLifecycleTest, CleanupOldMetricsRemovesExpired) {
	// Set very short retention period
	monitor_->set_metrics_retention_period(std::chrono::minutes(0));

	monitor_->record_query_metrics(
		make_query("old", std::chrono::microseconds(100), true));

	monitor_->cleanup_old_metrics();

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 0u);
}

TEST_F(MetricLifecycleTest, CleanupKeepsRecentMetrics) {
	monitor_->set_metrics_retention_period(std::chrono::minutes(60));

	monitor_->record_query_metrics(
		make_query("recent", std::chrono::microseconds(100), true));

	monitor_->cleanup_old_metrics();

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 1u);
}

//=============================================================================
// performance_monitor: Connection Metrics Tests
//=============================================================================

class ConnectionMetricsTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(ConnectionMetricsTest, UpdateConnectionCountStoresValues) {
	monitor_->update_connection_count(database_types::postgres, 5, 10);

	auto cm = monitor_->get_connection_metrics(database_types::postgres);
	EXPECT_EQ(cm.active_connections.load(), 5u);
	EXPECT_EQ(cm.total_connections.load(), 10u);
}

TEST_F(ConnectionMetricsTest, GetConnectionMetricsUnknownDbType) {
	auto cm = monitor_->get_connection_metrics(database_types::sqlite);
	EXPECT_EQ(cm.total_connections.load(), 0u);
	EXPECT_EQ(cm.active_connections.load(), 0u);
}

TEST_F(ConnectionMetricsTest, ConnectionSummaryIncludesMultipleTypes) {
	// get_performance_summary() requires at least one query in history
	// to reach connection metrics aggregation (early return if empty)
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(100), true));

	monitor_->update_connection_count(database_types::postgres, 3, 10);
	monitor_->update_connection_count(database_types::sqlite, 2, 5);

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_connections, 15u);
	EXPECT_EQ(summary.active_connections, 5u);
	EXPECT_DOUBLE_EQ(summary.connection_utilization, 5.0 / 15.0);
}

//=============================================================================
// performance_monitor: JSON and Dashboard Tests
//=============================================================================

class MetricsOutputTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(MetricsOutputTest, GetMetricsJsonContainsFields) {
	monitor_->record_query_metrics(
		make_query("q1", std::chrono::microseconds(1000), true));

	std::string json = monitor_->get_metrics_json();
	EXPECT_NE(json.find("total_queries"), std::string::npos);
	EXPECT_NE(json.find("successful_queries"), std::string::npos);
	EXPECT_NE(json.find("failed_queries"), std::string::npos);
	EXPECT_NE(json.find("avg_query_time_us"), std::string::npos);
	EXPECT_NE(json.find("error_rate"), std::string::npos);
	EXPECT_NE(json.find("active_connections"), std::string::npos);
}

TEST_F(MetricsOutputTest, GetMetricsJsonEmptyMonitor) {
	std::string json = monitor_->get_metrics_json();
	EXPECT_FALSE(json.empty());
	EXPECT_NE(json.find("total_queries"), std::string::npos);
}

//=============================================================================
// performance_alert Tests
//=============================================================================

class PerformanceAlertTest : public ::testing::Test {};

TEST_F(PerformanceAlertTest, ConstructionAndAccessors) {
	auto now = std::chrono::steady_clock::now();
	performance_alert alert(
		performance_alert::alert_type::high_latency,
		"Latency too high",
		now);

	EXPECT_EQ(alert.type(), performance_alert::alert_type::high_latency);
	EXPECT_EQ(alert.message(), "Latency too high");
	EXPECT_EQ(alert.timestamp(), now);
}

TEST_F(PerformanceAlertTest, AllAlertTypes) {
	auto now = std::chrono::steady_clock::now();
	const performance_alert::alert_type types[] = {
		performance_alert::alert_type::high_latency,
		performance_alert::alert_type::high_error_rate,
		performance_alert::alert_type::connection_pool_exhaustion,
		performance_alert::alert_type::slow_query,
		performance_alert::alert_type::memory_usage,
		performance_alert::alert_type::cpu_usage
	};

	for (auto type : types) {
		performance_alert alert(type, "test", now);
		EXPECT_EQ(alert.type(), type);
	}
}

//=============================================================================
// pool_metrics Tests
//=============================================================================

class PoolMetricsTest : public ::testing::Test {
protected:
	pool_metrics metrics_;
};

TEST_F(PoolMetricsTest, InitialStateIsZero) {
	EXPECT_EQ(metrics_.total_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.successful_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.failed_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.timeouts.load(), 0u);
	EXPECT_EQ(metrics_.current_active.load(), 0u);
	EXPECT_EQ(metrics_.current_queued.load(), 0u);
}

TEST_F(PoolMetricsTest, RecordSuccessfulAcquisition) {
	metrics_.record_acquisition(500, true);

	EXPECT_EQ(metrics_.total_acquisitions.load(), 1u);
	EXPECT_EQ(metrics_.successful_acquisitions.load(), 1u);
	EXPECT_EQ(metrics_.failed_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.total_wait_time_us.load(), 500u);
	EXPECT_EQ(metrics_.min_wait_time_us.load(), 500u);
	EXPECT_EQ(metrics_.max_wait_time_us.load(), 500u);
}

TEST_F(PoolMetricsTest, RecordFailedAcquisition) {
	metrics_.record_acquisition(0, false);

	EXPECT_EQ(metrics_.total_acquisitions.load(), 1u);
	EXPECT_EQ(metrics_.successful_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.failed_acquisitions.load(), 1u);
}

TEST_F(PoolMetricsTest, RecordMultipleAcquisitionsTracksMinMax) {
	metrics_.record_acquisition(100, true);
	metrics_.record_acquisition(500, true);
	metrics_.record_acquisition(200, true);

	EXPECT_EQ(metrics_.total_acquisitions.load(), 3u);
	EXPECT_EQ(metrics_.successful_acquisitions.load(), 3u);
	EXPECT_EQ(metrics_.total_wait_time_us.load(), 800u);
	EXPECT_EQ(metrics_.min_wait_time_us.load(), 100u);
	EXPECT_EQ(metrics_.max_wait_time_us.load(), 500u);
}

TEST_F(PoolMetricsTest, RecordTimeout) {
	metrics_.record_timeout();
	metrics_.record_timeout();

	EXPECT_EQ(metrics_.timeouts.load(), 2u);
}

TEST_F(PoolMetricsTest, UpdateActiveTracksPeak) {
	metrics_.update_active(1);
	metrics_.update_active(1);
	metrics_.update_active(1); // peak = 3
	metrics_.update_active(-1); // current = 2

	EXPECT_EQ(metrics_.current_active.load(), 2u);
	EXPECT_EQ(metrics_.peak_active.load(), 3u);
}

TEST_F(PoolMetricsTest, UpdateQueuedTracksPeak) {
	metrics_.update_queued(1);
	metrics_.update_queued(1);
	metrics_.update_queued(1); // peak = 3
	metrics_.update_queued(-1);
	metrics_.update_queued(-1); // current = 1

	EXPECT_EQ(metrics_.current_queued.load(), 1u);
	EXPECT_EQ(metrics_.peak_queued.load(), 3u);
}

TEST_F(PoolMetricsTest, RecordHealthCheck) {
	metrics_.record_health_check(0);
	metrics_.record_health_check(2);
	metrics_.record_health_check(1);

	EXPECT_EQ(metrics_.health_checks_performed.load(), 3u);
	EXPECT_EQ(metrics_.unhealthy_connections_removed.load(), 3u);
}

TEST_F(PoolMetricsTest, AverageWaitTime) {
	metrics_.record_acquisition(100, true);
	metrics_.record_acquisition(300, true);

	// average_wait_time_us divides by total_acquisitions (2), not successful
	EXPECT_DOUBLE_EQ(metrics_.average_wait_time_us(), 200.0);
}

TEST_F(PoolMetricsTest, AverageWaitTimeZeroWhenNoAcquisitions) {
	EXPECT_DOUBLE_EQ(metrics_.average_wait_time_us(), 0.0);
}

TEST_F(PoolMetricsTest, SuccessRate) {
	metrics_.record_acquisition(100, true);
	metrics_.record_acquisition(200, true);
	metrics_.record_acquisition(0, false);

	EXPECT_NEAR(metrics_.success_rate(), 66.666, 0.01);
}

TEST_F(PoolMetricsTest, SuccessRateNoAcquisitions) {
	EXPECT_DOUBLE_EQ(metrics_.success_rate(), 100.0);
}

TEST_F(PoolMetricsTest, ResetClearsStatistics) {
	metrics_.record_acquisition(500, true);
	metrics_.record_timeout();
	metrics_.update_active(3);
	metrics_.record_health_check(1);

	metrics_.reset();

	EXPECT_EQ(metrics_.total_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.successful_acquisitions.load(), 0u);
	EXPECT_EQ(metrics_.timeouts.load(), 0u);
	EXPECT_EQ(metrics_.total_wait_time_us.load(), 0u);
	EXPECT_EQ(metrics_.min_wait_time_us.load(), UINT64_MAX);
	EXPECT_EQ(metrics_.max_wait_time_us.load(), 0u);
	EXPECT_EQ(metrics_.health_checks_performed.load(), 0u);
	// current_active should remain (reflects current state)
	EXPECT_EQ(metrics_.current_active.load(), 3u);
	// peak_active reset to current_active
	EXPECT_EQ(metrics_.peak_active.load(), 3u);
}

//=============================================================================
// prometheus_exporter Tests
//=============================================================================

class PrometheusExporterTest : public ::testing::Test {};

TEST_F(PrometheusExporterTest, FormatContainsMetricNames) {
	prometheus_exporter exporter("localhost", 9090);
	performance_summary summary;
	summary.total_queries = 100;
	summary.avg_query_time = std::chrono::microseconds(500);
	summary.error_rate = 0.02;
	summary.active_connections = 5;

	std::string output = exporter.format_prometheus_metrics(summary);
	EXPECT_NE(output.find("database_queries_total 100"), std::string::npos);
	EXPECT_NE(output.find("database_query_duration_microseconds 500"), std::string::npos);
	EXPECT_NE(output.find("database_error_rate 0.02"), std::string::npos);
	EXPECT_NE(output.find("database_connections_active 5"), std::string::npos);
}

TEST_F(PrometheusExporterTest, ExportMetricsReturnsTrue) {
	prometheus_exporter exporter("localhost", 9090);
	performance_summary summary;

	EXPECT_TRUE(exporter.export_metrics(summary));
}

TEST_F(PrometheusExporterTest, ExportAlertsReturnsTrue) {
	prometheus_exporter exporter("localhost", 9090);
	std::vector<performance_alert> alerts;
	alerts.emplace_back(
		performance_alert::alert_type::slow_query, "test",
		std::chrono::steady_clock::now());

	EXPECT_TRUE(exporter.export_alerts(alerts));
}

//=============================================================================
// query_timer RAII Tests
//=============================================================================

class QueryTimerTest : public ::testing::Test {
protected:
	std::shared_ptr<performance_monitor> monitor_;

	void SetUp() override {
		monitor_ = std::make_shared<performance_monitor>();
	}
};

TEST_F(QueryTimerTest, DestructorRecordsMetrics) {
	{
		query_timer timer("SELECT 1", database_types::postgres, monitor_);
		// Timer will record on destruction
	}

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 1u);
	EXPECT_EQ(summary.successful_queries, 1u);
}

TEST_F(QueryTimerTest, SetErrorMarksFailure) {
	{
		query_timer timer("SELECT bad", database_types::postgres, monitor_);
		timer.set_error("syntax error");
	}

	auto summary = monitor_->get_performance_summary();
	EXPECT_EQ(summary.total_queries, 1u);
	EXPECT_EQ(summary.failed_queries, 1u);
}

TEST_F(QueryTimerTest, SetRowsAffected) {
	{
		query_timer timer("INSERT INTO test VALUES (1)", database_types::sqlite, monitor_);
		timer.set_rows_affected(42);
	}

	auto recent = monitor_->get_recent_queries(std::chrono::minutes(5));
	ASSERT_EQ(recent.size(), 1u);
	EXPECT_EQ(recent[0].rows_affected, 42u);
}
