// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_monitoring_adapter.cpp
 * @brief Unit tests for monitoring_adapter (Phase 3)
 *
 * These are lightweight API tests that verify the adapter interface
 * without requiring the actual monitoring_system to be available.
 *
 * For full integration testing with monitoring_system, run the
 * integration test suite instead.
 */

#include <kcenon/database/integrated/adapters/monitoring_adapter.h>
#include <chrono>
#include <iostream>
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
		try { \
			test_##name(); \
			std::cout << "PASSED\n"; \
			tests_passed++; \
		} catch (const std::exception& e) { \
			std::cout << "FAILED: " << e.what() << "\n"; \
			tests_failed++; \
		} \
	} while(0)

#define ASSERT_TRUE(condition) \
	if (!(condition)) { \
		throw std::runtime_error("Assertion failed: " #condition); \
	}

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

//==============================================================================
// API Verification Tests (No External Dependencies)
//==============================================================================

// Test 1: Configuration construction
TEST(configuration_construction) {
	db_monitoring_config config;
	config.enable_metrics = true;
	config.enable_profiling = true;
	config.enable_health_checks = true;
	config.metrics_interval = std::chrono::seconds(60);
	config.enable_prometheus_export = true;

	// Should be able to create config without errors
	ASSERT_TRUE(config.enable_metrics == true);
	ASSERT_TRUE(config.metrics_interval.count() == 60);
}

// Test 2: Adapter construction
TEST(adapter_construction) {
	db_monitoring_config config;
	config.enable_metrics = true;

	// Should be able to construct adapter
	monitoring_adapter monitor(config);

	// Construction should succeed
	// Note: Actual initialization may require monitoring_system
}

// Test 3: API availability - basic methods
TEST(api_availability_basic) {
	db_monitoring_config config;
	config.enable_metrics = false; // Disable to avoid external dependencies

	monitoring_adapter monitor(config);

	// These methods should be available (may no-op if monitoring disabled)
	monitor.record_connection_acquired();
	monitor.record_connection_released();
	monitor.record_query_execution(std::chrono::microseconds(100), true);
	monitor.record_transaction_begin();
	monitor.record_transaction_commit();
	monitor.record_transaction_rollback();
	monitor.update_pool_stats(1, 5, 10);

	// If we get here without crash, API is available
}

// Test 4: API availability - metrics retrieval
TEST(api_availability_metrics) {
	db_monitoring_config config;
	config.enable_metrics = false;

	monitoring_adapter monitor(config);

	// Should be able to call these methods (may return default/empty values)
	auto metrics_result = monitor.get_database_metrics();
	// Result may be ok or error depending on initialization state

	auto metrics_snapshot_result = monitor.get_metrics();
	// Result may be ok or error depending on initialization state
}

// Test 5: API availability - health check
TEST(api_availability_health) {
	db_monitoring_config config;
	config.enable_health_checks = false;

	monitoring_adapter monitor(config);

	// Should be able to call health check (may return unhealthy if not init)
	auto health_result = monitor.check_health();
	// Result structure should be valid even if health check failed
}

// Test 6: API availability - prometheus export
TEST(api_availability_prometheus) {
	db_monitoring_config config;
	config.enable_prometheus_export = false;

	monitoring_adapter monitor(config);

	// Should be able to call prometheus export (may return empty string)
	auto prometheus_text = monitor.export_prometheus_metrics();
	// Should return a string (may be empty)
}

// Test 7: API availability - reset
TEST(api_availability_reset) {
	db_monitoring_config config;
	config.enable_metrics = false;

	monitoring_adapter monitor(config);

	// Should be able to call reset without crash
	monitor.reset();
}

// Test 8: Multiple adapter instances
TEST(multiple_instances) {
	db_monitoring_config config1;
	config1.enable_metrics = false;

	db_monitoring_config config2;
	config2.enable_metrics = false;

	// Should be able to create multiple adapters
	monitoring_adapter monitor1(config1);
	monitoring_adapter monitor2(config2);

	// Both should be usable
	monitor1.record_connection_acquired();
	monitor2.record_connection_acquired();
}

// Test 9: Move semantics
TEST(move_semantics) {
	db_monitoring_config config;
	config.enable_metrics = false;

	monitoring_adapter monitor1(config);

	// Should support move construction
	monitoring_adapter monitor2(std::move(monitor1));

	// Moved-to instance should be usable
	monitor2.record_connection_acquired();
}

// Test 10: Destructor safety
TEST(destructor_safety) {
	// Test that adapter can be constructed and destroyed safely
	{
		db_monitoring_config config;
		config.enable_metrics = false;

		monitoring_adapter monitor(config);
		monitor.record_connection_acquired();

		// Destructor will be called here
	}

	// Should not crash
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
	std::cout << "=== Monitoring Adapter API Tests (Phase 3) ===\n";
	std::cout << "Note: These tests verify API availability only.\n";
	std::cout << "For full integration testing, run integration test suite.\n\n";

	RUN_TEST(configuration_construction);
	RUN_TEST(adapter_construction);
	RUN_TEST(api_availability_basic);
	RUN_TEST(api_availability_metrics);
	RUN_TEST(api_availability_health);
	RUN_TEST(api_availability_prometheus);
	RUN_TEST(api_availability_reset);
	RUN_TEST(multiple_instances);
	RUN_TEST(move_semantics);
	RUN_TEST(destructor_safety);

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
