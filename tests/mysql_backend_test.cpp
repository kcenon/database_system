/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * MySQL Backend Tests
 *
 * Tests for mysql_backend covering:
 * - Type identification
 * - Operations fail without initialization
 * - backend_base lifecycle guards
 * - Conditional: real connection tests (when USE_MYSQL defined)
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "database/backends/mysql_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

// =============================================================================
// Test Fixture
// =============================================================================

class MySQLBackendTest : public ::testing::Test {
protected:
    std::unique_ptr<mysql_backend> backend_;
    connection_config test_config_;

    void SetUp() override
    {
        backend_ = std::make_unique<mysql_backend>();
        test_config_.host = "localhost";
        test_config_.port = 3306;
        test_config_.database = "test_db";
        test_config_.username = "root";
        test_config_.password = "";
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

TEST_F(MySQLBackendTest, TypeReturnsMySQL)
{
    EXPECT_EQ(backend_->type(), database_types::mysql);
}

TEST_F(MySQLBackendTest, BackendNameIsCorrect)
{
    EXPECT_STREQ(mysql_backend::backend_name(), "mysql_backend");
}

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(MySQLBackendTest, InitiallyNotInitialized)
{
    EXPECT_FALSE(backend_->is_initialized());
}

TEST_F(MySQLBackendTest, InitiallyNotInTransaction)
{
    EXPECT_FALSE(backend_->in_transaction());
}

// =============================================================================
// Operations Without Initialization Tests
// =============================================================================

TEST_F(MySQLBackendTest, InsertQueryFailsWithoutInit)
{
    auto result = backend_->insert_query("INSERT INTO test VALUES (1)");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, UpdateQueryFailsWithoutInit)
{
    auto result = backend_->update_query("UPDATE test SET x = 1");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, DeleteQueryFailsWithoutInit)
{
    auto result = backend_->delete_query("DELETE FROM test WHERE id = 1");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, SelectQueryFailsWithoutInit)
{
    auto result = backend_->select_query("SELECT * FROM test");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, ExecuteQueryFailsWithoutInit)
{
    auto result = backend_->execute_query("CREATE TABLE test (id INT)");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, BeginTransactionFailsWithoutInit)
{
    auto result = backend_->begin_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, CommitTransactionFailsWithoutInit)
{
    auto result = backend_->commit_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MySQLBackendTest, RollbackTransactionFailsWithoutInit)
{
    auto result = backend_->rollback_transaction();
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Lifecycle Guard Tests (via backend_base)
// =============================================================================

TEST_F(MySQLBackendTest, ShutdownWithoutInitIsNoOp)
{
    auto result = backend_->shutdown();
    EXPECT_TRUE(result.is_ok());
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_F(MySQLBackendTest, CreateReturnsValidBackend)
{
    auto backend = mysql_backend::create();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::mysql);
    EXPECT_FALSE(backend->is_initialized());
}

// =============================================================================
// Connection Info Tests
// =============================================================================

TEST_F(MySQLBackendTest, ConnectionInfoBeforeInit)
{
    auto info = backend_->connection_info();
    SUCCEED();
}

TEST_F(MySQLBackendTest, LastErrorBeforeInit)
{
    auto error = backend_->last_error();
    SUCCEED();
}

// =============================================================================
// Real Connection Tests (conditional on USE_MYSQL)
// =============================================================================

#ifdef USE_MYSQL

TEST_F(MySQLBackendTest, ConnectToLocalMySQL)
{
    connection_config config;
    config.host = "localhost";
    config.port = 3306;
    config.database = "mysql"; // Default system database
    config.username = "root";
    config.password = "";

    auto result = backend_->initialize(config);
    if (!result.is_ok()) {
        GTEST_SKIP() << "Local MySQL not available: " << backend_->last_error();
    }

    EXPECT_TRUE(backend_->is_initialized());
}

TEST_F(MySQLBackendTest, CRUDOperationsOnMySQL)
{
    connection_config config;
    config.host = "localhost";
    config.port = 3306;
    config.database = "mysql";
    config.username = "root";
    config.password = "";

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local MySQL not available";
    }

    // Create temp table
    ASSERT_TRUE(backend_->execute_query(
        "CREATE TEMPORARY TABLE mysql_test (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255))").is_ok());

    // Insert
    auto insert_result = backend_->insert_query(
        "INSERT INTO mysql_test (name) VALUES ('test_item')");
    EXPECT_TRUE(insert_result.is_ok());

    // Select
    auto select_result = backend_->select_query(
        "SELECT * FROM mysql_test WHERE name = 'test_item'");
    ASSERT_TRUE(select_result.is_ok());
    EXPECT_GE(select_result.value().size(), 1u);

    // Update
    auto update_result = backend_->update_query(
        "UPDATE mysql_test SET name = 'updated' WHERE name = 'test_item'");
    EXPECT_TRUE(update_result.is_ok());

    // Delete
    auto delete_result = backend_->delete_query(
        "DELETE FROM mysql_test WHERE name = 'updated'");
    EXPECT_TRUE(delete_result.is_ok());
}

TEST_F(MySQLBackendTest, TransactionsOnMySQL)
{
    connection_config config;
    config.host = "localhost";
    config.port = 3306;
    config.database = "mysql";
    config.username = "root";
    config.password = "";

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local MySQL not available";
    }

    // InnoDB required for transactions
    ASSERT_TRUE(backend_->execute_query(
        "CREATE TEMPORARY TABLE mysql_tx_test (id INT AUTO_INCREMENT PRIMARY KEY, val VARCHAR(255)) ENGINE=InnoDB").is_ok());

    // Test commit
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    EXPECT_TRUE(backend_->in_transaction());
    backend_->insert_query("INSERT INTO mysql_tx_test (val) VALUES ('committed')");
    EXPECT_TRUE(backend_->commit_transaction().is_ok());
    EXPECT_FALSE(backend_->in_transaction());

    // Test rollback
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    backend_->insert_query("INSERT INTO mysql_tx_test (val) VALUES ('rolled_back')");
    EXPECT_TRUE(backend_->rollback_transaction().is_ok());

    auto result = backend_->select_query(
        "SELECT * FROM mysql_tx_test WHERE val = 'rolled_back'");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

#endif // USE_MYSQL

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
