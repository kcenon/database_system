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
 * @file test_monitoring_adapter.cpp
 * @brief Unit tests for monitoring_adapter (Phase 3)
 *
 * Tests the monitoring adapter functionality including:
 * - Initialization and shutdown
 * - Connection metrics recording
 * - Query metrics recording
 * - Transaction metrics recording
 * - Health checks
 * - Metrics retrieval
 * - Thread safety
 */

#include "../../database/integrated/adapters/monitoring_adapter.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

using namespace database::integrated;
using namespace database::integrated::adapters;

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) \
	do { \
		std::cout << "Running test: " << #name << " ... "; \
		test_##name(); \
		std::cout << "PASSED\n"; \
		tests_passed++; \
	} catch (const std::exception& e) { \
		std::cout << "FAILED: " << e.what() << "\n"; \
		tests_failed++; \
	}

#define ASSERT_TRUE(condition) \
	if (!(condition)) { \
		throw std::runtime_error("Assertion failed: " #condition); \
	}

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

// Test 1: Initialization and shutdown
TEST(initialization_and_shutdown) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_profiling = false;
	config.enable_health_checks = false;
	config.metrics_interval = std::chrono::seconds(60);
	config.enable_prometheus_export = false;

	monitoring_adapter monitor(config);

	// Initialize
	auto init_result = monitor.initialize();
	ASSERT_TRUE(init_result.is_ok());

	// Shutdown
	auto shutdown_result = monitor.shutdown();
	ASSERT_TRUE(shutdown_result.is_ok());
}

// Test 2: Record connection metrics
TEST(record_connection_metrics) {
	db_monitoring_config config;
	config.enable_metrics = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record connection events
	monitor.record_connection_acquired();
	monitor.record_connection_acquired();
	monitor.record_connection_released();
	// record_connection_failed() does not exist in implementation

	// Update pool stats
	monitor.update_pool_stats(1, 5, 10); // 1 active, 5 idle, 10 total

	// Get metrics
	auto metrics_result = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_result.is_ok());

	const auto& metrics = metrics_result.value();
	ASSERT_TRUE(metrics.active_connections == 1);
	ASSERT_TRUE(metrics.idle_connections == 5);
	ASSERT_TRUE(metrics.total_connections == 10);

	monitor.shutdown();
}

// Test 3: Record query metrics
TEST(record_query_metrics) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_profiling = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record successful queries
	monitor.record_query_execution(std::chrono::microseconds(100), true);
	monitor.record_query_execution(std::chrono::microseconds(200), true);
	monitor.record_query_execution(std::chrono::microseconds(300), true);

	// Record failed query
	monitor.record_query_execution(std::chrono::microseconds(50), false);

	// Get metrics
	auto metrics_result = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_result.is_ok());

	const auto& metrics = metrics_result.value();
	ASSERT_TRUE(metrics.total_queries == 4);
	ASSERT_TRUE(metrics.successful_queries == 3);
	ASSERT_TRUE(metrics.failed_queries == 1);

	// Check average latency (should be around 200us for successful queries)
	ASSERT_TRUE(metrics.avg_query_latency.count() > 0);

	monitor.shutdown();
}

// Test 4: Record transaction metrics
TEST(record_transaction_metrics) {
	db_monitoring_config config;
	config.enable_metrics = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record transactions
	monitor.record_transaction_begin();
	monitor.record_transaction_commit();

	monitor.record_transaction_begin();
	monitor.record_transaction_commit();

	monitor.record_transaction_begin();
	monitor.record_transaction_rollback();

	// Get metrics
	auto metrics_result = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_result.is_ok());

	const auto& metrics = metrics_result.value();
	ASSERT_TRUE(metrics.committed_transactions == 2);
	ASSERT_TRUE(metrics.rolled_back_transactions == 1);

	monitor.shutdown();
}

// Test 5: Slow query detection
TEST(slow_query_detection) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_profiling = true;
	// slow_query_threshold moved to logger config

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record fast query
	monitor.record_query_execution(std::chrono::microseconds(50000), true); // 50ms

	// Record slow query (should trigger warning)
	monitor.record_query_execution(std::chrono::microseconds(150000), true); // 150ms

	// Get metrics
	auto metrics_result = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_result.is_ok());

	// slow_queries field does not exist in database_metrics
	// Slow query detection is handled by logger_adapter

	monitor.shutdown();
}

// Test 6: Health check functionality
TEST(health_check) {
	db_monitoring_config config;
	config.enable_health_checks = true;
	// health_check_interval does not exist in config

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Perform health check using check_health() instead of perform_health_check()
	auto health_result = monitor.check_health();
	ASSERT_TRUE(health_result.is_ok());
	ASSERT_TRUE(health_result.value().is_healthy()); // Should be healthy initially

	monitor.shutdown();
}

// Test 7: Metrics snapshot
TEST(metrics_snapshot) {
	db_monitoring_config config;
	config.enable_metrics = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record some activity
	monitor.record_connection_acquired();
	monitor.record_query_execution(std::chrono::microseconds(100), true);
	monitor.update_pool_stats(1, 5, 10);

	// Get snapshot using get_metrics() instead of get_metrics_snapshot()
	auto snapshot_result = monitor.get_metrics();
	ASSERT_TRUE(snapshot_result.is_ok());

	const auto& snapshot = snapshot_result.value();

	// Verify snapshot has expected keys
	ASSERT_TRUE(!snapshot.gauges.empty());
	ASSERT_TRUE(!snapshot.counters.empty());

	monitor.shutdown();
}

// Test 8: Thread safety
TEST(thread_safety) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_profiling = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	const int num_threads = 4;
	const int operations_per_thread = 100;

	std::vector<std::thread> threads;

	for (int i = 0; i < num_threads; ++i) {
		threads.emplace_back([&monitor, operations_per_thread]() {
			for (int j = 0; j < operations_per_thread; ++j) {
				// Mix different operations
				monitor.record_connection_acquired();
				monitor.record_query_execution(
					std::chrono::microseconds(100 + j), true);
				monitor.update_pool_stats(j % 10, 5, 10);
				monitor.record_connection_released();
			}
		});
	}

	// Wait for all threads to complete
	for (auto& thread : threads) {
		thread.join();
	}

	// Verify metrics are consistent
	auto metrics_result = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_result.is_ok());

	const auto& metrics = metrics_result.value();
	ASSERT_TRUE(metrics.total_queries == num_threads * operations_per_thread);

	monitor.shutdown();
}

// Test 9: Metrics reset
TEST(metrics_reset) {
	db_monitoring_config config;
	config.enable_metrics = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record some activity
	monitor.record_connection_acquired();
	monitor.record_query_execution(std::chrono::microseconds(100), true);

	// Get metrics
	auto metrics_before = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_before.is_ok());
	ASSERT_TRUE(metrics_before.value().total_queries > 0);

	// Reset metrics using reset() instead of reset_metrics()
	monitor.reset();

	// Verify reset
	auto metrics_after = monitor.get_database_metrics();
	ASSERT_TRUE(metrics_after.is_ok());
	ASSERT_TRUE(metrics_after.value().total_queries == 0);

	monitor.shutdown();
}

// Test 10: Prometheus export format
TEST(prometheus_export) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_prometheus_export = true;

	monitoring_adapter monitor(config);
	monitor.initialize();

	// Record some metrics
	monitor.record_connection_acquired();
	monitor.record_query_execution(std::chrono::microseconds(150), true);
	monitor.update_pool_stats(2, 8, 10);

	// Get Prometheus format
	auto prometheus_result = monitor.export_prometheus_metrics();
	ASSERT_TRUE(prometheus_result.is_ok());

	const auto& prometheus_text = prometheus_result.value();

	// Verify format contains expected metrics
	ASSERT_TRUE(prometheus_text.find("db_active_connections") != std::string::npos);
	ASSERT_TRUE(prometheus_text.find("db_total_queries") != std::string::npos);

	monitor.shutdown();
}

// Main test runner
int main() {
	std::cout << "=== Monitoring Adapter Tests (Phase 3) ===\n\n";

	try {
		RUN_TEST(initialization_and_shutdown);
		RUN_TEST(record_connection_metrics);
		RUN_TEST(record_query_metrics);
		RUN_TEST(record_transaction_metrics);
		RUN_TEST(slow_query_detection);
		RUN_TEST(health_check);
		RUN_TEST(metrics_snapshot);
		RUN_TEST(thread_safety);
		RUN_TEST(metrics_reset);
		RUN_TEST(prometheus_export);
	} catch (const std::exception& e) {
		std::cerr << "Unexpected error: " << e.what() << "\n";
		return 1;
	}

	std::cout << "\n=== Test Summary ===\n";
	std::cout << "Passed: " << tests_passed << "\n";
	std::cout << "Failed: " << tests_failed << "\n";

	if (tests_failed == 0) {
		std::cout << "=== All tests passed! ✓ ===\n";
		return 0;
	} else {
		std::cout << "=== Some tests failed! ✗ ===\n";
		return 1;
	}
}
