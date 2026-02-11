/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * PostgreSQL Backend Tests
 *
 * Tests for postgresql_backend covering:
 * - Type identification
 * - Operations fail without initialization
 * - Connection string building
 * - backend_base lifecycle guards
 * - Conditional: real connection tests (when USE_POSTGRESQL defined)
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "database/backends/postgresql_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

// =============================================================================
// Test Fixture
// =============================================================================

class PostgreSQLBackendTest : public ::testing::Test {
protected:
    std::unique_ptr<postgresql_backend> backend_;
    connection_config test_config_;

    void SetUp() override
    {
        backend_ = std::make_unique<postgresql_backend>();
        test_config_.host = "localhost";
        test_config_.port = 5432;
        test_config_.database = "test_db";
        test_config_.username = "test_user";
        test_config_.password = "test_pass";
    }

    void TearDown() override
    {
        if (backend_ && backend_->is_initialized()) {
            backend_->shutdown();
        }
    }
};

// =============================================================================
// Type Identification Tests
// =============================================================================

TEST_F(PostgreSQLBackendTest, TypeReturnsPostgres)
{
    EXPECT_EQ(backend_->type(), database_types::postgres);
}

TEST_F(PostgreSQLBackendTest, BackendNameIsCorrect)
{
    EXPECT_STREQ(postgresql_backend::backend_name(), "postgresql_backend");
}

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(PostgreSQLBackendTest, InitiallyNotInitialized)
{
    EXPECT_FALSE(backend_->is_initialized());
}

TEST_F(PostgreSQLBackendTest, InitiallyNotInTransaction)
{
    EXPECT_FALSE(backend_->in_transaction());
}

// =============================================================================
// Operations Without Initialization Tests
// =============================================================================

TEST_F(PostgreSQLBackendTest, InsertQueryFailsWithoutInit)
{
    auto result = backend_->insert_query("INSERT INTO test VALUES (1)");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, UpdateQueryFailsWithoutInit)
{
    auto result = backend_->update_query("UPDATE test SET x = 1");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, DeleteQueryFailsWithoutInit)
{
    auto result = backend_->delete_query("DELETE FROM test WHERE id = 1");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, SelectQueryFailsWithoutInit)
{
    auto result = backend_->select_query("SELECT * FROM test");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, ExecuteQueryFailsWithoutInit)
{
    auto result = backend_->execute_query("CREATE TABLE test (id INT)");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, BeginTransactionFailsWithoutInit)
{
    auto result = backend_->begin_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, CommitTransactionFailsWithoutInit)
{
    auto result = backend_->commit_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, RollbackTransactionFailsWithoutInit)
{
    auto result = backend_->rollback_transaction();
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Lifecycle Guard Tests (via backend_base)
// =============================================================================

TEST_F(PostgreSQLBackendTest, ShutdownWithoutInitIsNoOp)
{
    auto result = backend_->shutdown();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(PostgreSQLBackendTest, DoubleInitializationRejected)
{
#ifdef USE_POSTGRESQL
    // Only test if actual PostgreSQL is available
    auto first = backend_->initialize(test_config_);
    if (first.is_ok()) {
        auto second = backend_->initialize(test_config_);
        EXPECT_FALSE(second.is_ok());
    }
#else
    GTEST_SKIP() << "PostgreSQL support not compiled";
#endif
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_F(PostgreSQLBackendTest, CreateReturnsValidBackend)
{
    auto backend = postgresql_backend::create();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::postgres);
    EXPECT_FALSE(backend->is_initialized());
}

// =============================================================================
// Connection Info Tests
// =============================================================================

TEST_F(PostgreSQLBackendTest, ConnectionInfoBeforeInit)
{
    auto info = backend_->connection_info();
    // Implementation may return empty or partial info before init
    // Just verify no crash
    SUCCEED();
}

TEST_F(PostgreSQLBackendTest, LastErrorBeforeInit)
{
    // Should not crash, may return empty
    auto error = backend_->last_error();
    SUCCEED();
}

// =============================================================================
// Real Connection Tests (conditional on USE_POSTGRESQL)
// =============================================================================

#ifdef USE_POSTGRESQL

TEST_F(PostgreSQLBackendTest, ConnectToLocalPostgres)
{
    connection_config config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "postgres"; // Default database
    config.username = "postgres";
    config.password = "";

    auto result = backend_->initialize(config);
    if (!result.is_ok()) {
        GTEST_SKIP() << "Local PostgreSQL not available: " << backend_->last_error();
    }

    EXPECT_TRUE(backend_->is_initialized());
    auto info = backend_->connection_info();
    EXPECT_FALSE(info.empty());
}

TEST_F(PostgreSQLBackendTest, CRUDOperationsOnPostgres)
{
    connection_config config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "postgres";
    config.username = "postgres";
    config.password = "";

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local PostgreSQL not available";
    }

    // Create temp table
    ASSERT_TRUE(backend_->execute_query(
        "CREATE TEMP TABLE pg_test (id SERIAL PRIMARY KEY, name TEXT)").is_ok());

    // Insert
    auto insert_result = backend_->insert_query(
        "INSERT INTO pg_test (name) VALUES ('test_item')");
    EXPECT_TRUE(insert_result.is_ok());

    // Select
    auto select_result = backend_->select_query(
        "SELECT * FROM pg_test WHERE name = 'test_item'");
    ASSERT_TRUE(select_result.is_ok());
    EXPECT_GE(select_result.value().size(), 1u);

    // Update
    auto update_result = backend_->update_query(
        "UPDATE pg_test SET name = 'updated' WHERE name = 'test_item'");
    EXPECT_TRUE(update_result.is_ok());

    // Delete
    auto delete_result = backend_->delete_query(
        "DELETE FROM pg_test WHERE name = 'updated'");
    EXPECT_TRUE(delete_result.is_ok());
}

TEST_F(PostgreSQLBackendTest, TransactionsOnPostgres)
{
    connection_config config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "postgres";
    config.username = "postgres";
    config.password = "";

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local PostgreSQL not available";
    }

    ASSERT_TRUE(backend_->execute_query(
        "CREATE TEMP TABLE pg_tx_test (id SERIAL, val TEXT)").is_ok());

    // Test commit
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    EXPECT_TRUE(backend_->in_transaction());
    backend_->insert_query("INSERT INTO pg_tx_test (val) VALUES ('committed')");
    EXPECT_TRUE(backend_->commit_transaction().is_ok());
    EXPECT_FALSE(backend_->in_transaction());

    // Test rollback
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    backend_->insert_query("INSERT INTO pg_tx_test (val) VALUES ('rolled_back')");
    EXPECT_TRUE(backend_->rollback_transaction().is_ok());

    // Verify rolled back data doesn't exist
    auto result = backend_->select_query(
        "SELECT * FROM pg_tx_test WHERE val = 'rolled_back'");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

#endif // USE_POSTGRESQL

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
