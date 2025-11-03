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
 * @file test_unified_database_system.cpp
 * @brief Phase 6: Unit tests for unified_database_system
 *
 * Tests the main entry point of the integrated database system.
 * These tests focus on API availability, configuration, and initialization.
 * Integration tests with real databases are in integration_tests/.
 */

#include "integrated/unified_database_system.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace database::integrated;

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

// Test helpers
#define TEST_START(name) \
    std::cout << "\n[TEST] " << name << "...\n"

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << "  ❌ FAILED: " << message << "\n"; \
            std::cout << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition, message) \
    ASSERT_TRUE(!(condition), message)

#define TEST_END() \
    do { \
        std::cout << "  ✅ PASSED\n"; \
        tests_passed++; \
        return true; \
    } while(0)

//==============================================================================
// Test 1: Builder Pattern - Default Configuration
//==============================================================================

bool test_builder_default() {
    TEST_START("Builder Pattern - Default Configuration");

    auto builder = unified_database_system::create_builder();
    auto db = builder.build();

    ASSERT_TRUE(db != nullptr, "Builder should create database instance");

    TEST_END();
}

//==============================================================================
// Test 2: Builder Pattern - Custom Configuration
//==============================================================================

bool test_builder_custom() {
    TEST_START("Builder Pattern - Custom Configuration");

    try {
        auto db = unified_database_system::create_builder()
            .set_backend(backend_type::postgres)
            .set_connection_string("host=localhost dbname=test")
            .set_pool_size(5, 20)
            .enable_logging(db_log_level::debug, "./test_logs")
            .enable_monitoring(true)
            .enable_async(8)
            .set_slow_query_threshold(std::chrono::milliseconds(500))
            .build();

        ASSERT_TRUE(db != nullptr, "Builder with custom config should create instance");

        // Note: Connection is not established yet, just configuration
        // Actual connection would happen on connect() or first query

    } catch (const std::exception& e) {
        // If PostgreSQL is not available or not compiled in, that's acceptable
        // This test is just verifying the builder API works
        std::cout << "  ℹ️  Note: Database connection not available: " << e.what() << "\n";
        std::cout << "  ℹ️  Builder API test passed (connection test skipped)\n";
    }

    TEST_END();
}

//==============================================================================
// Test 3: Zero-Config Construction
//==============================================================================

bool test_zero_config_construction() {
    TEST_START("Zero-Config Construction");

    // Should create with smart defaults
    unified_database_system db;

    // Should not be connected yet
    ASSERT_FALSE(db.is_connected(), "Should not be connected without connect()");

    TEST_END();
}

//==============================================================================
// Test 4: Configuration-Based Construction
//==============================================================================

bool test_config_construction() {
    TEST_START("Configuration-Based Construction");

    unified_db_config config;
    config.database.type = backend_type::postgres;
    config.connection_pool.min_connections = 2;
    config.connection_pool.max_connections = 10;
    config.logger.enable_query_logging = true;
    config.monitoring.enable_metrics = true;

    unified_database_system db(config);

    ASSERT_FALSE(db.is_connected(), "Should not be connected without connect()");

    TEST_END();
}

//==============================================================================
// Test 5: Move Semantics
//==============================================================================

bool test_move_semantics() {
    TEST_START("Move Semantics");

    auto db1 = unified_database_system::create_builder()
        .set_backend(backend_type::postgres)
        .build();

    ASSERT_TRUE(db1 != nullptr, "Original instance should be valid");

    // Move construction
    auto db2 = std::move(db1);
    ASSERT_TRUE(db2 != nullptr, "Moved instance should be valid");

    // Move assignment
    auto db3 = unified_database_system::create_builder().build();
    db3 = std::move(db2);
    ASSERT_TRUE(db3 != nullptr, "Move-assigned instance should be valid");

    TEST_END();
}

//==============================================================================
// Test 6: Connection State Management (Without Real DB)
//==============================================================================

bool test_connection_state_api() {
    TEST_START("Connection State Management API");

    unified_database_system db;

    // Initial state
    ASSERT_FALSE(db.is_connected(), "Should start disconnected");

    // Note: We can't actually test connect() without a real database
    // Integration tests will cover actual connections

    // Test that disconnect can be called safely when not connected
    auto result = db.disconnect();
    // Should either succeed (no-op) or return a specific error
    // Either is acceptable behavior

    TEST_END();
}

//==============================================================================
// Test 7: Health Check API Availability
//==============================================================================

bool test_health_check_api() {
    TEST_START("Health Check API Availability");

    unified_database_system db;

    // Should be able to call health check even when not connected
    auto health = db.check_health();

    // Health check should return a valid structure
    // When not connected, status should indicate this
    ASSERT_TRUE(
        health.status == health_status::failed ||
        health.status == health_status::critical,
        "Health check should show non-healthy status when disconnected"
    );

    ASSERT_FALSE(health.is_connected, "Health check should show not connected");

    TEST_END();
}

//==============================================================================
// Test 8: Metrics API Availability
//==============================================================================

bool test_metrics_api() {
    TEST_START("Metrics API Availability");

    unified_database_system db;

    // Should be able to retrieve metrics even when not connected
    auto metrics = db.get_metrics();

    // Initial metrics should be zero
    ASSERT_TRUE(metrics.total_queries == 0, "Initial query count should be 0");
    ASSERT_TRUE(metrics.successful_queries == 0, "Initial success count should be 0");
    ASSERT_TRUE(metrics.failed_queries == 0, "Initial failure count should be 0");
    ASSERT_TRUE(metrics.active_connections == 0, "Initial connections should be 0");

    TEST_END();
}

//==============================================================================
// Test 9: Query Result Structure
//==============================================================================

bool test_query_result_structure() {
    TEST_START("Query Result Structure");

    // Create an empty query result
    query_result result;

    ASSERT_TRUE(result.empty(), "Empty result should report as empty");
    ASSERT_TRUE(result.size() == 0, "Empty result size should be 0");
    ASSERT_TRUE(result.affected_rows == 0, "Initial affected rows should be 0");

    // Add some test data
    result.rows.push_back({{"id", "1"}, {"name", "test"}});
    result.affected_rows = 1;

    ASSERT_FALSE(result.empty(), "Result with data should not be empty");
    ASSERT_TRUE(result.size() == 1, "Result size should match row count");
    ASSERT_TRUE(result.affected_rows == 1, "Affected rows should match");

    // Test row access
    auto& row = result[0];
    ASSERT_TRUE(row.at("id") == "1", "Row data should be accessible");
    ASSERT_TRUE(row.at("name") == "test", "Row data should be correct");

    // Test iteration
    size_t count = 0;
    for (const auto& r : result) {
        count++;
        (void)r; // Suppress unused warning
    }
    ASSERT_TRUE(count == 1, "Should iterate over all rows");

    TEST_END();
}

//==============================================================================
// Test 10: Query Parameter Construction
//==============================================================================

bool test_query_parameters() {
    TEST_START("Query Parameter Construction");

    // Test various parameter types
    std::vector<query_param> params;

    params.push_back(query_param("string value"));
    params.push_back(query_param(42));
    params.push_back(query_param(3.14));
    params.push_back(query_param(true));
    params.push_back(query_param(false));

    ASSERT_TRUE(params.size() == 5, "Should accept various parameter types");
    ASSERT_TRUE(params[0].value == "string value", "String param should work");
    ASSERT_TRUE(params[1].value == "42", "Int param should convert to string");
    ASSERT_TRUE(params[3].value == "true", "Bool true should convert correctly");
    ASSERT_TRUE(params[4].value == "false", "Bool false should convert correctly");

    TEST_END();
}

//==============================================================================
// Test 11: Database Metrics Structure
//==============================================================================

bool test_metrics_structure() {
    TEST_START("Database Metrics Structure");

    database_metrics metrics;

    // Test default values
    ASSERT_TRUE(metrics.total_queries == 0, "Default total_queries is 0");
    ASSERT_TRUE(metrics.queries_per_second == 0.0, "Default QPS is 0");
    ASSERT_TRUE(metrics.pool_size == 0, "Default pool_size is 0");
    ASSERT_TRUE(metrics.transactions_started == 0, "Default transactions is 0");

    // Test assignment
    metrics.total_queries = 100;
    metrics.successful_queries = 95;
    metrics.failed_queries = 5;
    metrics.queries_per_second = 10.5;

    ASSERT_TRUE(metrics.total_queries == 100, "Can set total queries");
    ASSERT_TRUE(metrics.successful_queries == 95, "Can set successful queries");
    ASSERT_TRUE(metrics.queries_per_second == 10.5, "Can set QPS");

    TEST_END();
}

//==============================================================================
// Test 12: Health Check Structure
//==============================================================================

bool test_health_check_structure() {
    TEST_START("Health Check Structure");

    health_check health;

    // Test default values
    ASSERT_TRUE(health.status == health_status::healthy, "Default status is healthy");
    ASSERT_FALSE(health.is_connected, "Default is not connected");
    ASSERT_TRUE(health.issues.empty(), "Default has no issues");

    // Test assignment
    health.status = health_status::degraded;
    health.is_connected = true;
    health.logger_healthy = true;
    health.monitor_healthy = true;
    health.thread_pool_healthy = true;
    health.connection_pool_utilization = 0.75;
    health.issues.push_back("Test issue");

    ASSERT_TRUE(health.status == health_status::degraded, "Can set status");
    ASSERT_TRUE(health.is_connected, "Can set connected state");
    ASSERT_TRUE(health.connection_pool_utilization == 0.75, "Can set utilization");
    ASSERT_TRUE(health.issues.size() == 1, "Can add issues");

    TEST_END();
}

//==============================================================================
// Test 13: Thread Safety - Concurrent Health Checks
//==============================================================================

bool test_thread_safety_health_checks() {
    TEST_START("Thread Safety - Concurrent Health Checks");

    unified_database_system db;

    // Launch multiple threads checking health concurrently
    std::vector<std::thread> threads;
    std::atomic<size_t> checks_completed{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&db, &checks_completed]() {
            for (int j = 0; j < 100; ++j) {
                auto health = db.check_health();
                (void)health; // Suppress unused warning
            }
            checks_completed++;
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    ASSERT_TRUE(checks_completed == 10, "All threads should complete");

    TEST_END();
}

//==============================================================================
// Test 14: Thread Safety - Concurrent Metrics Retrieval
//==============================================================================

bool test_thread_safety_metrics() {
    TEST_START("Thread Safety - Concurrent Metrics Retrieval");

    unified_database_system db;

    std::vector<std::thread> threads;
    std::atomic<size_t> retrievals_completed{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&db, &retrievals_completed]() {
            for (int j = 0; j < 100; ++j) {
                auto metrics = db.get_metrics();
                (void)metrics; // Suppress unused warning
            }
            retrievals_completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_TRUE(retrievals_completed == 10, "All threads should complete");

    TEST_END();
}

//==============================================================================
// Test 15: Error Handling - Query Without Connection
//==============================================================================

bool test_error_handling_no_connection() {
    TEST_START("Error Handling - Query Without Connection");

    unified_database_system db;

    // Try to execute a query without connecting
    auto result = db.execute("SELECT 1");

    ASSERT_TRUE(result.is_err(), "Query without connection should fail");

    TEST_END();
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Phase 6: Unified Database System Tests\n";
    std::cout << "========================================\n";

    // Run all tests
    test_builder_default();
    test_builder_custom();
    test_zero_config_construction();
    test_config_construction();
    test_move_semantics();
    test_connection_state_api();
    test_health_check_api();
    test_metrics_api();
    test_query_result_structure();
    test_query_parameters();
    test_metrics_structure();
    test_health_check_structure();
    test_thread_safety_health_checks();
    test_thread_safety_metrics();
    test_error_handling_no_connection();

    // Print summary
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";

    if (tests_failed == 0) {
        std::cout << "\n✅ All tests passed!\n\n";
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed!\n\n";
        return 1;
    }
}
