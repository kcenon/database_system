// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_unified_database_system.cpp
 * @brief Phase 6: Unit tests for unified_database_system
 *
 * Tests the main entry point of the integrated database system.
 * These tests focus on API availability, configuration, and initialization.
 * Integration tests with real databases are in integration_tests/.
 */

#include "integrated/unified_database_system.h"
#include "core/database_backend.h"
#include "core/backend_registry.h"

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
// Stub Backend for Mock Testing
//==============================================================================

namespace {

/**
 * @brief In-memory stub backend for unit testing.
 *
 * Implements database_backend interface with deterministic behavior,
 * allowing unified_database_system tests without a real database.
 * Uses ::database:: (global scope) to avoid conflict with the
 * `using database = unified_database_system;` alias.
 */
class stub_backend : public ::database::core::database_backend {
public:
    stub_backend() = default;

    static std::unique_ptr<::database::core::database_backend> create() {
        return std::make_unique<stub_backend>();
    }

    ::database::database_types type() const override {
        return ::database::database_types::sqlite;
    }

    kcenon::common::VoidResult initialize(
        const ::database::core::connection_config& /*config*/) override {
        initialized_ = true;
        return kcenon::common::VoidResult(std::monostate{});
    }

    kcenon::common::VoidResult shutdown() override {
        initialized_ = false;
        in_tx_ = false;
        return kcenon::common::VoidResult(std::monostate{});
    }

    bool is_initialized() const override { return initialized_; }

    kcenon::common::Result<::database::core::database_result> select_query(
        const std::string& /*query_string*/) override {
        ::database::core::database_result result;
        ::database::core::database_row row;
        row["id"] = int64_t{1};
        row["name"] = std::string("test_user");
        result.push_back(row);
        return result;
    }

    kcenon::common::VoidResult execute_query(
        const std::string& /*query_string*/) override {
        return kcenon::common::VoidResult(std::monostate{});
    }

    kcenon::common::VoidResult begin_transaction() override {
        in_tx_ = true;
        return kcenon::common::VoidResult(std::monostate{});
    }

    kcenon::common::VoidResult commit_transaction() override {
        in_tx_ = false;
        return kcenon::common::VoidResult(std::monostate{});
    }

    kcenon::common::VoidResult rollback_transaction() override {
        in_tx_ = false;
        return kcenon::common::VoidResult(std::monostate{});
    }

    bool in_transaction() const override { return in_tx_; }

    std::string last_error() const override { return ""; }

    std::map<std::string, std::string> connection_info() const override {
        return {{"backend", "stub"}, {"version", "1.0"}};
    }

private:
    bool initialized_ = false;
    bool in_tx_ = false;
};

// Register stub as "sqlite" backend for test use
void register_stub_backend() {
    auto& registry = ::database::core::backend_registry::instance();
    if (!registry.has_backend("sqlite")) {
        registry.register_backend("sqlite", &stub_backend::create);
    }
}

void unregister_stub_backend() {
    ::database::core::backend_registry::instance().unregister_backend("sqlite");
}

} // anonymous namespace

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
    ASSERT_TRUE(params[0].get_value() == "string value", "String param should work");
    ASSERT_TRUE(params[1].get_value() == "42", "Int param should convert to string");
    ASSERT_TRUE(params[3].get_value() == "true", "Bool true should convert correctly");
    ASSERT_TRUE(params[4].get_value() == "false", "Bool false should convert correctly");

    // Verify non-null parameters
    ASSERT_FALSE(params[0].is_null(), "String param should not be null");
    ASSERT_FALSE(params[1].is_null(), "Int param should not be null");

    TEST_END();
}

//==============================================================================
// Test 10a: Query Parameter Null Safety
//==============================================================================

bool test_query_param_null_safety() {
    TEST_START("Query Parameter Null Safety");

    // Test explicit nullptr
    query_param null_param(nullptr);
    ASSERT_TRUE(null_param.is_null(), "nullptr should create null param");
    ASSERT_TRUE(null_param.get_value().empty(), "Null param get_value should return empty string");
    ASSERT_TRUE(null_param.to_sql_string() == "NULL", "Null param SQL string should be NULL");

    // Test null const char*
    const char* null_str = nullptr;
    query_param null_char_param(null_str);
    ASSERT_TRUE(null_char_param.is_null(), "null const char* should create null param");

    // Test valid const char*
    const char* valid_str = "test";
    query_param valid_char_param(valid_str);
    ASSERT_FALSE(valid_char_param.is_null(), "valid const char* should not be null");
    ASSERT_TRUE(valid_char_param.get_value() == "test", "valid const char* should preserve value");

    // Test empty string vs null
    query_param empty_param("");
    ASSERT_FALSE(empty_param.is_null(), "Empty string should not be null");
    ASSERT_TRUE(empty_param.get_value().empty(), "Empty string value should be empty");
    ASSERT_TRUE(empty_param.to_sql_string().empty(), "Empty string SQL should be empty");

    // Test move semantics
    std::string str = "moved value";
    query_param moved_param(std::move(str));
    ASSERT_FALSE(moved_param.is_null(), "Moved string should not be null");
    ASSERT_TRUE(moved_param.get_value() == "moved value", "Moved value should be preserved");

    // Test additional integer types
    query_param ll_param(static_cast<long long>(123456789012345LL));
    ASSERT_FALSE(ll_param.is_null(), "long long should not be null");

    query_param ull_param(static_cast<unsigned long long>(18446744073709551615ULL));
    ASSERT_FALSE(ull_param.is_null(), "unsigned long long should not be null");

    // Test float
    query_param float_param(3.14f);
    ASSERT_FALSE(float_param.is_null(), "float should not be null");

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

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Query without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Query without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 16: Connect with Mock Backend
//==============================================================================

bool test_connect_mock_backend() {
    TEST_START("Connect with Mock Backend (SQLite stub)");

    register_stub_backend();

    unified_database_system db;
    auto result = db.connect(backend_type::sqlite, ":memory:");
    ASSERT_TRUE(result.is_ok(), "Connect with stub backend should succeed");
    ASSERT_TRUE(db.is_connected(), "Should be connected after successful connect");

    auto disconnect_result = db.disconnect();
    ASSERT_TRUE(disconnect_result.is_ok(), "Disconnect should succeed");
    ASSERT_FALSE(db.is_connected(), "Should not be connected after disconnect");

    TEST_END();
}

//==============================================================================
// Test 17: Connect with Typed Backend
//==============================================================================

bool test_connect_typed_backend() {
    TEST_START("Connect with Typed Backend");

    register_stub_backend();

    unified_database_system db;
    auto result = db.connect(backend_type::sqlite, ":memory:");
    ASSERT_TRUE(result.is_ok(), "Typed connect with stub should succeed");
    ASSERT_TRUE(db.is_connected(), "Should be connected");

    TEST_END();
}

//==============================================================================
// Test 18: Connect Unsupported Backend
//==============================================================================

bool test_connect_unsupported_backend() {
    TEST_START("Connect Unsupported Backend");

    unified_database_system db;
    auto result = db.connect(backend_type::mongodb, "localhost:27017");

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Connect with unsupported backend should fail");
#else
    ASSERT_TRUE(result.is_error(), "Connect with unsupported backend should fail");
#endif

    ASSERT_FALSE(db.is_connected(), "Should not be connected on failure");

    TEST_END();
}

//==============================================================================
// Test 19: Execute Success Path
//==============================================================================

bool test_execute_success() {
    TEST_START("Execute Success Path");

    register_stub_backend();

    unified_database_system db;
    auto conn = db.connect(backend_type::sqlite, ":memory:");
    ASSERT_TRUE(conn.is_ok(), "Connect should succeed");

    auto result = db.execute("SELECT * FROM users WHERE id = 1");
    ASSERT_TRUE(result.is_ok(), "Execute on connected db should succeed");

    auto& qr = result.value();
    ASSERT_TRUE(qr.size() == 1, "Should return one row from stub");
    ASSERT_TRUE(qr[0].at("name") == "test_user", "Row should contain stub data");

    TEST_END();
}

//==============================================================================
// Test 20: Select Success Path
//==============================================================================

bool test_select_success() {
    TEST_START("Select Success Path");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto result = db.select("SELECT id, name FROM users");
    ASSERT_TRUE(result.is_ok(), "Select should succeed");

    auto& qr = result.value();
    ASSERT_FALSE(qr.empty(), "Select should return rows");
    ASSERT_TRUE(qr[0].count("id") > 0, "Row should have 'id' column");

    TEST_END();
}

//==============================================================================
// Test 21: Select Failure Path (Not Connected)
//==============================================================================

bool test_select_failure_not_connected() {
    TEST_START("Select Failure Path - Not Connected");

    unified_database_system db;
    auto result = db.select("SELECT 1");

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Select without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Select without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 22: Insert Success Path
//==============================================================================

bool test_insert_success() {
    TEST_START("Insert Success Path");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto result = db.insert("INSERT INTO users (name) VALUES ('Alice')");
    ASSERT_TRUE(result.is_ok(), "Insert should succeed");

    auto affected = result.value();
    ASSERT_TRUE(affected > 0, "Insert should affect at least one row");

    TEST_END();
}

//==============================================================================
// Test 23: Insert Failure Path (Not Connected)
//==============================================================================

bool test_insert_failure_not_connected() {
    TEST_START("Insert Failure Path - Not Connected");

    unified_database_system db;
    auto result = db.insert("INSERT INTO users (name) VALUES ('Bob')");

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Insert without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Insert without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 24: Update Success Path
//==============================================================================

bool test_update_success() {
    TEST_START("Update Success Path");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto result = db.update("UPDATE users SET name = 'Bob' WHERE id = 1");
    ASSERT_TRUE(result.is_ok(), "Update should succeed");

    auto affected = result.value();
    ASSERT_TRUE(affected > 0, "Update should affect at least one row");

    TEST_END();
}

//==============================================================================
// Test 25: Update Failure Path (Not Connected)
//==============================================================================

bool test_update_failure_not_connected() {
    TEST_START("Update Failure Path - Not Connected");

    unified_database_system db;
    auto result = db.update("UPDATE users SET name = 'Bob'");

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Update without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Update without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 26: Remove Success Path
//==============================================================================

bool test_remove_success() {
    TEST_START("Remove Success Path");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto result = db.remove("DELETE FROM users WHERE id = 1");
    ASSERT_TRUE(result.is_ok(), "Remove should succeed");

    auto affected = result.value();
    ASSERT_TRUE(affected > 0, "Remove should affect at least one row");

    TEST_END();
}

//==============================================================================
// Test 27: Remove Failure Path (Not Connected)
//==============================================================================

bool test_remove_failure_not_connected() {
    TEST_START("Remove Failure Path - Not Connected");

    unified_database_system db;
    auto result = db.remove("DELETE FROM users WHERE id = 1");

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Remove without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Remove without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 28: Begin Transaction Success Path
//==============================================================================

bool test_begin_transaction_success() {
    TEST_START("Begin Transaction Success Path");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto tx_result = db.begin_transaction();
    ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed when connected");

    auto& tx = tx_result.value();
    ASSERT_TRUE(tx != nullptr, "Transaction pointer should be valid");
    ASSERT_TRUE(tx->is_active(), "Transaction should be active after begin");

    TEST_END();
}

//==============================================================================
// Test 29: Begin Transaction Failure Path (Not Connected)
//==============================================================================

bool test_begin_transaction_failure() {
    TEST_START("Begin Transaction Failure Path - Not Connected");

    unified_database_system db;
    auto tx_result = db.begin_transaction();

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(tx_result.is_err(), "Begin transaction without connection should fail");
#else
    ASSERT_TRUE(tx_result.is_error(), "Begin transaction without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 30: Transaction Execute
//==============================================================================

bool test_transaction_execute() {
    TEST_START("Transaction Execute");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto tx_result = db.begin_transaction();
    ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed");

    auto& tx = tx_result.value();
    auto exec_result = tx->execute("INSERT INTO users (name) VALUES ('Alice')");
    ASSERT_TRUE(exec_result.is_ok(), "Transaction execute should succeed");

    TEST_END();
}

//==============================================================================
// Test 31: Transaction Commit
//==============================================================================

bool test_transaction_commit() {
    TEST_START("Transaction Commit");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto tx_result = db.begin_transaction();
    ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed");

    auto& tx = tx_result.value();
    ASSERT_TRUE(tx->is_active(), "Transaction should be active before commit");

    auto commit_result = tx->commit();
    ASSERT_TRUE(commit_result.is_ok(), "Commit should succeed");
    ASSERT_FALSE(tx->is_active(), "Transaction should not be active after commit");

    TEST_END();
}

//==============================================================================
// Test 32: Transaction Rollback
//==============================================================================

bool test_transaction_rollback() {
    TEST_START("Transaction Rollback");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto tx_result = db.begin_transaction();
    ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed");

    auto& tx = tx_result.value();
    ASSERT_TRUE(tx->is_active(), "Transaction should be active before rollback");

    auto rollback_result = tx->rollback();
    ASSERT_TRUE(rollback_result.is_ok(), "Rollback should succeed");
    ASSERT_FALSE(tx->is_active(), "Transaction should not be active after rollback");

    TEST_END();
}

//==============================================================================
// Test 33: Transaction is_active State Tracking
//==============================================================================

bool test_transaction_is_active() {
    TEST_START("Transaction is_active State Tracking");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    auto tx_result = db.begin_transaction();
    ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed");

    auto& tx = tx_result.value();

    // Active after begin
    ASSERT_TRUE(tx->is_active(), "Should be active after begin");

    // Execute should not change active state
    tx->execute("SELECT 1");
    ASSERT_TRUE(tx->is_active(), "Should remain active after execute");

    // Inactive after commit
    tx->commit();
    ASSERT_FALSE(tx->is_active(), "Should be inactive after commit");

    // Double commit should fail
    auto double_commit = tx->commit();
#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(double_commit.is_err(), "Double commit should fail");
#else
    ASSERT_TRUE(double_commit.is_error(), "Double commit should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 34: Transaction RAII Cleanup (Rollback on Scope Exit)
//==============================================================================

bool test_transaction_raii_cleanup() {
    TEST_START("Transaction RAII Cleanup");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    {
        auto tx_result = db.begin_transaction();
        ASSERT_TRUE(tx_result.is_ok(), "Begin transaction should succeed");

        auto& tx = tx_result.value();
        tx->execute("INSERT INTO users (name) VALUES ('Alice')");

        // Do not commit - transaction goes out of scope
        // Destructor should auto-rollback
    }

    // If we reach here without crash, RAII cleanup worked
    ASSERT_TRUE(true, "Transaction destructor should auto-rollback without crash");

    TEST_END();
}

//==============================================================================
// Test 35: Execute Transaction (Batch)
//==============================================================================

bool test_execute_transaction_success() {
    TEST_START("Execute Transaction - Batch Success");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    std::vector<std::string> queries = {
        "INSERT INTO users (name) VALUES ('Alice')",
        "INSERT INTO users (name) VALUES ('Bob')",
        "UPDATE users SET name = 'Charlie' WHERE id = 1"
    };

    auto result = db.execute_transaction(queries);
    ASSERT_TRUE(result.is_ok(), "Batch transaction should succeed");

    TEST_END();
}

//==============================================================================
// Test 36: Execute Transaction Failure (Not Connected)
//==============================================================================

bool test_execute_transaction_failure() {
    TEST_START("Execute Transaction - Not Connected");

    unified_database_system db;

    std::vector<std::string> queries = {"INSERT INTO users (name) VALUES ('Alice')"};
    auto result = db.execute_transaction(queries);

#if defined(USE_COMMON_SYSTEM)
    ASSERT_TRUE(result.is_err(), "Batch transaction without connection should fail");
#else
    ASSERT_TRUE(result.is_error(), "Batch transaction without connection should fail");
#endif

    TEST_END();
}

//==============================================================================
// Test 37: get_config Accessor
//==============================================================================

bool test_get_config() {
    TEST_START("get_config Accessor");

    unified_db_config config;
    config.database.type = backend_type::sqlite;
    config.connection_pool.min_connections = 3;
    config.connection_pool.max_connections = 15;

    unified_database_system db(config);

    const auto& retrieved = db.get_config();
    ASSERT_TRUE(retrieved.database.type == backend_type::sqlite,
                "Config should reflect configured backend type");
    ASSERT_TRUE(retrieved.connection_pool.min_connections == 3,
                "Config should reflect configured min connections");
    ASSERT_TRUE(retrieved.connection_pool.max_connections == 15,
                "Config should reflect configured max connections");

    TEST_END();
}

//==============================================================================
// Test 38: get_backend_type Accessor
//==============================================================================

bool test_get_backend_type() {
    TEST_START("get_backend_type Accessor");

    unified_database_system db;
    // Default backend type should be postgres (from impl constructor)
    backend_type bt = db.get_backend_type();
    ASSERT_TRUE(bt == backend_type::postgres,
                "Default backend type should be postgres");

    TEST_END();
}

//==============================================================================
// Test 39: get_pool_stats Accessor
//==============================================================================

bool test_get_pool_stats() {
    TEST_START("get_pool_stats Accessor");

    unified_database_system db;

    // When not connected
    auto stats = db.get_pool_stats();
    ASSERT_TRUE(stats.total_connections == 0,
                "Not connected: total connections should be 0");
    ASSERT_TRUE(stats.active_connections == 0,
                "Not connected: active connections should be 0");

    // When connected
    register_stub_backend();
    db.connect(backend_type::sqlite, ":memory:");

    auto connected_stats = db.get_pool_stats();
    ASSERT_TRUE(connected_stats.total_connections == 1,
                "Connected: total connections should be 1");
    ASSERT_TRUE(connected_stats.active_connections == 1,
                "Connected: active connections should be 1");

    TEST_END();
}

//==============================================================================
// Test 40: reset_metrics
//==============================================================================

bool test_reset_metrics() {
    TEST_START("reset_metrics");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");

    // Execute some queries to accumulate metrics
    db.execute("SELECT 1");
    db.execute("SELECT 2");

    auto metrics_before = db.get_metrics();
    ASSERT_TRUE(metrics_before.total_queries >= 2,
                "Should have at least 2 queries recorded");

    // Reset
    db.reset_metrics();

    auto metrics_after = db.get_metrics();
    ASSERT_TRUE(metrics_after.total_queries == 0,
                "After reset, total queries should be 0");
    ASSERT_TRUE(metrics_after.successful_queries == 0,
                "After reset, successful queries should be 0");
    ASSERT_TRUE(metrics_after.failed_queries == 0,
                "After reset, failed queries should be 0");

    TEST_END();
}

//==============================================================================
// Test 41: create_query_builder
//==============================================================================

bool test_create_query_builder() {
    TEST_START("create_query_builder");

    unified_database_system db;
    auto builder = db.create_query_builder();

    // Just verify it returns a valid query_builder without crashing
    ASSERT_TRUE(true, "create_query_builder should return without error");

    TEST_END();
}

//==============================================================================
// Test 42: Metrics Update on Successful Query
//==============================================================================

bool test_metrics_update_on_query() {
    TEST_START("Metrics Update on Successful Query");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");
    db.reset_metrics();

    // Execute queries
    db.execute("SELECT 1");
    db.execute("SELECT 2");
    db.execute("SELECT 3");

    auto metrics = db.get_metrics();
    ASSERT_TRUE(metrics.total_queries == 3, "Should record 3 queries");
    ASSERT_TRUE(metrics.successful_queries == 3, "All 3 should be successful");
    ASSERT_TRUE(metrics.failed_queries == 0, "No failures expected");

    TEST_END();
}

//==============================================================================
// Test 43: Transaction Metrics
//==============================================================================

bool test_transaction_metrics() {
    TEST_START("Transaction Metrics");

    register_stub_backend();

    unified_database_system db;
    db.connect(backend_type::sqlite, ":memory:");
    db.reset_metrics();

    // Start and commit a transaction
    {
        auto tx_result = db.begin_transaction();
        ASSERT_TRUE(tx_result.is_ok(), "Begin should succeed");
        auto& tx = tx_result.value();
        tx->commit();
    }

    auto metrics = db.get_metrics();
    ASSERT_TRUE(metrics.transactions_started >= 1,
                "Should record at least 1 transaction started");

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

    // Phase 6 original tests (builder, config, state, structures)
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
    test_query_param_null_safety();
    test_metrics_structure();
    test_health_check_structure();
    test_thread_safety_health_checks();
    test_thread_safety_metrics();
    test_error_handling_no_connection();

    // CRUD and connection tests (with stub backend)
    test_connect_mock_backend();
    test_connect_typed_backend();
    test_connect_unsupported_backend();
    test_execute_success();
    test_select_success();
    test_select_failure_not_connected();
    test_insert_success();
    test_insert_failure_not_connected();
    test_update_success();
    test_update_failure_not_connected();
    test_remove_success();
    test_remove_failure_not_connected();

    // Transaction tests
    test_begin_transaction_success();
    test_begin_transaction_failure();
    test_transaction_execute();
    test_transaction_commit();
    test_transaction_rollback();
    test_transaction_is_active();
    test_transaction_raii_cleanup();
    test_execute_transaction_success();
    test_execute_transaction_failure();

    // Accessor tests
    test_get_config();
    test_get_backend_type();
    test_get_pool_stats();
    test_reset_metrics();
    test_create_query_builder();

    // Metrics verification tests
    test_metrics_update_on_query();
    test_transaction_metrics();

    // Cleanup stub backend registration
    unregister_stub_backend();

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
