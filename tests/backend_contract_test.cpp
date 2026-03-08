/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Backend Interface Contract Tests
 *
 * Tests the database_backend interface contract using mock_backend to verify:
 * - CRUD operations (insert, update, delete, select, execute)
 * - Transaction state machine (begin/commit/rollback)
 * - Error propagation through Result<T>
 * - Query recording and expectation matching
 * - Initialization failure simulation
 * - Mock builder presets
 * - Concurrent query execution
 * - Edge cases in transaction management
 */

#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "database/core/database_backend.h"
#include "mocks/mock_backend.h"

using namespace database;
using namespace database::core;
using namespace database::testing;

// =============================================================================
// Test Fixture
// =============================================================================

class BackendContractTest : public ::testing::Test {
protected:
    mock_backend backend_;
    connection_config test_config_;

    void SetUp() override
    {
        test_config_.host = "localhost";
        test_config_.port = 5432;
        test_config_.database = "test_db";
        test_config_.username = "user";
        test_config_.password = "pass";

        // Initialize mock for most tests
        backend_.set_database_type(database_types::postgres);
    }
};

// =============================================================================
// Initialization Contract Tests
// =============================================================================

TEST_F(BackendContractTest, InitializeSucceeds)
{
    auto result = backend_.initialize(test_config_);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(backend_.is_initialized());
}

TEST_F(BackendContractTest, InitializeStoresConnectionString)
{
    backend_.initialize(test_config_);
    auto conn_str = backend_.get_connection_string();
    EXPECT_FALSE(conn_str.empty());
    EXPECT_NE(conn_str.find("localhost"), std::string::npos);
    EXPECT_NE(conn_str.find("test_db"), std::string::npos);
}

TEST_F(BackendContractTest, InitializeFailureSimulation)
{
    backend_.simulate_initialization_failure("Custom error");
    auto result = backend_.initialize(test_config_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_FALSE(backend_.is_initialized());
}

TEST_F(BackendContractTest, InitializeFailureDefaultMessage)
{
    backend_.simulate_initialization_failure();
    auto result = backend_.initialize(test_config_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(backend_.last_error(), "Mock initialization failed");
}

TEST_F(BackendContractTest, ShutdownClearsState)
{
    backend_.initialize(test_config_);
    EXPECT_TRUE(backend_.is_initialized());

    auto result = backend_.shutdown();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(backend_.is_initialized());
}

TEST_F(BackendContractTest, SimulateShutdown)
{
    backend_.initialize(test_config_);
    backend_.simulate_shutdown();
    EXPECT_FALSE(backend_.is_initialized());
}

// =============================================================================
// Type Contract Tests
// =============================================================================

TEST_F(BackendContractTest, TypeReturnsConfiguredType)
{
    backend_.set_database_type(database_types::sqlite);
    EXPECT_EQ(backend_.type(), database_types::sqlite);
}

TEST_F(BackendContractTest, DefaultTypeIsNone)
{
    mock_backend fresh_backend;
    EXPECT_EQ(fresh_backend.type(), database_types::none);
}

// =============================================================================
// Insert Query Contract Tests
// =============================================================================

TEST_F(BackendContractTest, InsertQueryReturnsDefaultRowsAffected)
{
    backend_.set_default_rows_affected(5);
    auto result = backend_.insert_query("INSERT INTO users (name) VALUES ('John')");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 5u);
}

TEST_F(BackendContractTest, InsertQueryWithExpectation)
{
    backend_.expect_query("INSERT INTO users (name) VALUES ('John')")
        .will_return_rows(1);

    auto result = backend_.insert_query("INSERT INTO users (name) VALUES ('John')");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 1u);
}

TEST_F(BackendContractTest, InsertQueryWithError)
{
    backend_.expect_query("INSERT INTO users (name) VALUES ('John')")
        .will_fail("Duplicate key violation");

    auto result = backend_.insert_query("INSERT INTO users (name) VALUES ('John')");
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Update Query Contract Tests
// =============================================================================

TEST_F(BackendContractTest, UpdateQueryReturnsRowsAffected)
{
    backend_.expect_query("UPDATE users SET name = 'Jane' WHERE id = 1")
        .will_return_rows(1);

    auto result = backend_.update_query("UPDATE users SET name = 'Jane' WHERE id = 1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 1u);
}

TEST_F(BackendContractTest, UpdateQueryNoRowsAffected)
{
    backend_.expect_query("UPDATE users SET name = 'Jane' WHERE id = 999")
        .will_return_rows(0);

    auto result = backend_.update_query("UPDATE users SET name = 'Jane' WHERE id = 999");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 0u);
}

// =============================================================================
// Delete Query Contract Tests
// =============================================================================

TEST_F(BackendContractTest, DeleteQueryReturnsRowsAffected)
{
    backend_.expect_query("DELETE FROM users WHERE id = 1")
        .will_return_rows(1);

    auto result = backend_.delete_query("DELETE FROM users WHERE id = 1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 1u);
}

// =============================================================================
// Select Query Contract Tests
// =============================================================================

TEST_F(BackendContractTest, SelectQueryReturnsResults)
{
    database_result expected_data = {
        {{"id", int64_t(1)}, {"name", std::string("Alice")}},
        {{"id", int64_t(2)}, {"name", std::string("Bob")}}
    };

    backend_.expect_query("SELECT * FROM users")
        .will_return(expected_data);

    auto result = backend_.select_query("SELECT * FROM users");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 2u);

    auto& row0 = result.value()[0];
    EXPECT_EQ(std::get<int64_t>(row0.at("id")), 1);
    EXPECT_EQ(std::get<std::string>(row0.at("name")), "Alice");
}

TEST_F(BackendContractTest, SelectQueryReturnsEmptyResult)
{
    backend_.expect_query("SELECT * FROM empty_table")
        .will_return(database_result{});

    auto result = backend_.select_query("SELECT * FROM empty_table");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(BackendContractTest, SelectQueryWithDefaultResult)
{
    database_result default_data = {
        {{"count", int64_t(42)}}
    };
    backend_.set_default_select_result(default_data);

    auto result = backend_.select_query("SELECT COUNT(*) FROM anything");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 1u);
    EXPECT_EQ(std::get<int64_t>(result.value()[0].at("count")), 42);
}

// =============================================================================
// Execute Query Contract Tests
// =============================================================================

TEST_F(BackendContractTest, ExecuteQuerySucceeds)
{
    auto result = backend_.execute_query("CREATE TABLE test (id INTEGER)");
    EXPECT_TRUE(result.is_ok());
}

TEST_F(BackendContractTest, ExecuteQueryWithExpectedFailure)
{
    backend_.expect_query("DROP TABLE nonexistent")
        .will_fail("Table does not exist");

    auto result = backend_.execute_query("DROP TABLE nonexistent");
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Transaction State Machine Tests
// =============================================================================

TEST_F(BackendContractTest, TransactionInitiallyInactive)
{
    EXPECT_FALSE(backend_.in_transaction());
}

TEST_F(BackendContractTest, BeginTransactionActivates)
{
    EXPECT_TRUE(backend_.begin_transaction().is_ok());
    EXPECT_TRUE(backend_.in_transaction());
}

TEST_F(BackendContractTest, CommitTransactionDeactivates)
{
    backend_.begin_transaction();
    EXPECT_TRUE(backend_.commit_transaction().is_ok());
    EXPECT_FALSE(backend_.in_transaction());
}

TEST_F(BackendContractTest, RollbackTransactionDeactivates)
{
    backend_.begin_transaction();
    EXPECT_TRUE(backend_.rollback_transaction().is_ok());
    EXPECT_FALSE(backend_.in_transaction());
}

TEST_F(BackendContractTest, NestedBeginTransactionFails)
{
    backend_.begin_transaction();
    auto result = backend_.begin_transaction();
    EXPECT_FALSE(result.is_ok());
    // Should still be in transaction
    EXPECT_TRUE(backend_.in_transaction());
}

TEST_F(BackendContractTest, CommitWithoutTransactionFails)
{
    auto result = backend_.commit_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(BackendContractTest, RollbackWithoutTransactionFails)
{
    auto result = backend_.rollback_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(BackendContractTest, TransactionCycleBeginCommitBeginRollback)
{
    // First cycle: begin -> commit
    EXPECT_TRUE(backend_.begin_transaction().is_ok());
    EXPECT_TRUE(backend_.in_transaction());
    EXPECT_TRUE(backend_.commit_transaction().is_ok());
    EXPECT_FALSE(backend_.in_transaction());

    // Second cycle: begin -> rollback
    EXPECT_TRUE(backend_.begin_transaction().is_ok());
    EXPECT_TRUE(backend_.in_transaction());
    EXPECT_TRUE(backend_.rollback_transaction().is_ok());
    EXPECT_FALSE(backend_.in_transaction());
}

TEST_F(BackendContractTest, ShutdownClearsTransactionState)
{
    backend_.initialize(test_config_);
    backend_.begin_transaction();
    EXPECT_TRUE(backend_.in_transaction());

    backend_.shutdown();
    EXPECT_FALSE(backend_.in_transaction());
}

// =============================================================================
// Pattern Matching Tests
// =============================================================================

TEST_F(BackendContractTest, PatternMatchingWithRegex)
{
    database_result expected = {
        {{"id", int64_t(1)}, {"name", std::string("Test")}}
    };

    backend_.expect_pattern("SELECT.*FROM users.*")
        .will_return(expected);

    auto result = backend_.select_query("SELECT id, name FROM users WHERE id = 1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 1u);
}

TEST_F(BackendContractTest, AnyMatcherMatchesAll)
{
    backend_.expect_any().will_return_rows(42);

    auto r1 = backend_.insert_query("INSERT INTO a VALUES (1)");
    auto r2 = backend_.update_query("UPDATE b SET x = 1");
    auto r3 = backend_.delete_query("DELETE FROM c WHERE id = 1");

    EXPECT_TRUE(r1.is_ok());
    EXPECT_TRUE(r2.is_ok());
    EXPECT_TRUE(r3.is_ok());
}

// =============================================================================
// Query Recording Tests
// =============================================================================

TEST_F(BackendContractTest, RecordsExecutedQueries)
{
    backend_.insert_query("INSERT INTO t1 VALUES (1)");
    backend_.select_query("SELECT * FROM t1");
    backend_.update_query("UPDATE t1 SET x = 2");

    auto queries = backend_.get_executed_queries();
    EXPECT_EQ(queries.size(), 3u);
    EXPECT_EQ(queries[0], "INSERT INTO t1 VALUES (1)");
    EXPECT_EQ(queries[1], "SELECT * FROM t1");
    EXPECT_EQ(queries[2], "UPDATE t1 SET x = 2");
}

TEST_F(BackendContractTest, GetQueryCountTotal)
{
    backend_.insert_query("q1");
    backend_.insert_query("q2");
    backend_.select_query("q3");

    EXPECT_EQ(backend_.get_query_count(), 3u);
}

TEST_F(BackendContractTest, GetQueryCountByPattern)
{
    backend_.insert_query("INSERT INTO users VALUES (1)");
    backend_.insert_query("INSERT INTO orders VALUES (1)");
    backend_.select_query("SELECT * FROM users");

    EXPECT_EQ(backend_.get_query_count("INSERT"), 2u);
    EXPECT_EQ(backend_.get_query_count("users"), 2u);
    EXPECT_EQ(backend_.get_query_count("orders"), 1u);
}

TEST_F(BackendContractTest, ClearHistory)
{
    backend_.insert_query("q1");
    backend_.clear_history();
    EXPECT_EQ(backend_.get_query_count(), 0u);
    EXPECT_TRUE(backend_.get_executed_queries().empty());
}

TEST_F(BackendContractTest, ClearExpectations)
{
    backend_.expect_query("q1").will_return_rows(10);
    backend_.clear_expectations();

    // After clearing, should use default (1)
    auto result = backend_.insert_query("q1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 1u); // default rows affected
}

TEST_F(BackendContractTest, ResetClearsEverything)
{
    backend_.initialize(test_config_);
    backend_.begin_transaction();
    backend_.insert_query("q1");
    backend_.expect_query("q2").will_return_rows(5);

    backend_.reset();

    EXPECT_FALSE(backend_.is_initialized());
    EXPECT_FALSE(backend_.in_transaction());
    EXPECT_EQ(backend_.get_query_count(), 0u);
    EXPECT_TRUE(backend_.get_connection_string().empty());
}

// =============================================================================
// Expectation Verification Tests
// =============================================================================

TEST_F(BackendContractTest, VerifyAllExpectationsWhenAllMatched)
{
    backend_.expect_query("q1").will_return_rows(1).once();
    backend_.insert_query("q1");
    EXPECT_TRUE(backend_.verify_all_expectations());
}

TEST_F(BackendContractTest, VerifyExpectationsFailsWhenUnmatched)
{
    backend_.expect_query("q1").will_return_rows(1).once();
    // Don't execute q1
    EXPECT_FALSE(backend_.verify_all_expectations());
}

// =============================================================================
// Connection Info Tests
// =============================================================================

TEST_F(BackendContractTest, ConnectionInfoReturnsMap)
{
    backend_.initialize(test_config_);
    auto info = backend_.connection_info();

    EXPECT_FALSE(info.empty());
    EXPECT_EQ(info.at("type"), "mock");
    EXPECT_EQ(info.at("initialized"), "true");
}

TEST_F(BackendContractTest, LastErrorInitiallyEmpty)
{
    EXPECT_TRUE(backend_.last_error().empty());
}

// =============================================================================
// Mock Builder Preset Tests
// =============================================================================

TEST_F(BackendContractTest, EmptyDatabasePreset)
{
    auto db = mock_backend_builder::empty_database();
    auto result = db.select_query("SELECT * FROM anything");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(BackendContractTest, WithDataPreset)
{
    database_result test_data = {
        {{"id", int64_t(1)}, {"name", std::string("Test")}}
    };
    auto db = mock_backend_builder::with_data("users", test_data);

    auto result = db.select_query("SELECT * FROM users WHERE id = 1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 1u);
}

TEST_F(BackendContractTest, FailingDatabasePreset)
{
    auto db = mock_backend_builder::failing_database("DB is down");
    auto result = db.select_query("SELECT * FROM users");
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Data Type Tests (database_value variants)
// =============================================================================

TEST_F(BackendContractTest, AllDatabaseValueTypesInResult)
{
    database_result data = {{
        {"string_col", std::string("hello")},
        {"int_col", int64_t(42)},
        {"double_col", double(3.14)},
        {"bool_col", true},
        {"null_col", nullptr}
    }};

    backend_.expect_query("SELECT *").will_return(data);
    auto result = backend_.select_query("SELECT *");
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);

    auto& row = result.value()[0];
    EXPECT_EQ(std::get<std::string>(row.at("string_col")), "hello");
    EXPECT_EQ(std::get<int64_t>(row.at("int_col")), 42);
    EXPECT_DOUBLE_EQ(std::get<double>(row.at("double_col")), 3.14);
    EXPECT_EQ(std::get<bool>(row.at("bool_col")), true);
    EXPECT_EQ(std::get<std::nullptr_t>(row.at("null_col")), nullptr);
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_F(BackendContractTest, ConcurrentQueryExecution)
{
    constexpr int num_threads = 10;
    constexpr int queries_per_thread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t]() {
            for (int q = 0; q < queries_per_thread; ++q) {
                std::string query = "INSERT INTO t" + std::to_string(t) +
                                    " VALUES (" + std::to_string(q) + ")";
                backend_.insert_query(query);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(backend_.get_query_count(),
              static_cast<size_t>(num_threads * queries_per_thread));
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_F(BackendContractTest, MoveConstructor)
{
    backend_.initialize(test_config_);
    backend_.set_database_type(database_types::sqlite);
    backend_.insert_query("q1");

    mock_backend moved(std::move(backend_));

    EXPECT_TRUE(moved.is_initialized());
    EXPECT_EQ(moved.type(), database_types::sqlite);
    EXPECT_EQ(moved.get_query_count(), 1u);
}

TEST_F(BackendContractTest, MoveAssignment)
{
    backend_.initialize(test_config_);
    backend_.insert_query("q1");

    mock_backend target;
    target = std::move(backend_);

    EXPECT_TRUE(target.is_initialized());
    EXPECT_EQ(target.get_query_count(), 1u);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
