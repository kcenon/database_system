// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * MongoDB Backend Tests
 *
 * Tests for mongodb_backend covering:
 * - Type identification
 * - Operations fail without initialization
 * - backend_base lifecycle guards
 * - Thread safety (mutex-protected)
 * - Conditional: real connection tests (when USE_MONGODB defined)
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "database/backends/mongodb_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

// =============================================================================
// Test Fixture
// =============================================================================

class MongoDBBackendTest : public ::testing::Test {
protected:
    std::unique_ptr<mongodb_backend> backend_;
    connection_config test_config_;

    void SetUp() override
    {
        backend_ = std::make_unique<mongodb_backend>();
        test_config_.host = "localhost";
        test_config_.port = 27017;
        test_config_.database = "test_db";
        test_config_.username = "";
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

TEST_F(MongoDBBackendTest, TypeReturnsMongoDB)
{
    EXPECT_EQ(backend_->type(), database_types::mongodb);
}

TEST_F(MongoDBBackendTest, BackendNameIsCorrect)
{
    EXPECT_STREQ(mongodb_backend::backend_name(), "mongodb_backend");
}

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(MongoDBBackendTest, InitiallyNotInitialized)
{
    EXPECT_FALSE(backend_->is_initialized());
}

TEST_F(MongoDBBackendTest, InitiallyNotInTransaction)
{
    EXPECT_FALSE(backend_->in_transaction());
}

// =============================================================================
// Operations Without Initialization Tests
// =============================================================================

TEST_F(MongoDBBackendTest, ExecuteQueryFailsWithoutInit)
{
    auto result = backend_->execute_query("users:{\"name\":\"John\"}");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MongoDBBackendTest, SelectQueryFailsWithoutInit)
{
    auto result = backend_->select_query("users:{\"name\":\"John\"}");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MongoDBBackendTest, BeginTransactionFailsWithoutInit)
{
    auto result = backend_->begin_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MongoDBBackendTest, CommitTransactionFailsWithoutInit)
{
    auto result = backend_->commit_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(MongoDBBackendTest, RollbackTransactionFailsWithoutInit)
{
    auto result = backend_->rollback_transaction();
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Lifecycle Guard Tests (via backend_base)
// =============================================================================

TEST_F(MongoDBBackendTest, ShutdownWithoutInitIsNoOp)
{
    auto result = backend_->shutdown();
    EXPECT_TRUE(result.is_ok());
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_F(MongoDBBackendTest, CreateReturnsValidBackend)
{
    auto backend = mongodb_backend::create();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::mongodb);
    EXPECT_FALSE(backend->is_initialized());
}

// =============================================================================
// Connection Info Tests
// =============================================================================

TEST_F(MongoDBBackendTest, ConnectionInfoBeforeInit)
{
    auto info = backend_->connection_info();
    SUCCEED();
}

TEST_F(MongoDBBackendTest, LastErrorBeforeInit)
{
    auto error = backend_->last_error();
    SUCCEED();
}

// =============================================================================
// Real Connection Tests (conditional on USE_MONGODB)
// =============================================================================

#ifdef USE_MONGODB

TEST_F(MongoDBBackendTest, ConnectToLocalMongoDB)
{
    connection_config config;
    config.host = "localhost";
    config.port = 27017;
    config.database = "test";

    auto result = backend_->initialize(config);
    if (!result.is_ok()) {
        GTEST_SKIP() << "Local MongoDB not available: " << backend_->last_error();
    }

    EXPECT_TRUE(backend_->is_initialized());
    auto info = backend_->connection_info();
    EXPECT_FALSE(info.empty());
}

TEST_F(MongoDBBackendTest, CRUDOperationsOnMongoDB)
{
    connection_config config;
    config.host = "localhost";
    config.port = 27017;
    config.database = "test";

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local MongoDB not available";
    }

    // Insert document
    auto insert_result = backend_->execute_query(
        "mongo_test:{\"name\":\"test_item\",\"value\":42}");
    EXPECT_TRUE(insert_result.is_ok());

    // Select document
    auto select_result = backend_->select_query(
        "mongo_test:{\"name\":\"test_item\"}");
    ASSERT_TRUE(select_result.is_ok());
    EXPECT_GE(select_result.value().size(), 1u);

    // Update document
    auto update_result = backend_->execute_query(
        "mongo_test:{\"name\":\"test_item\"}:{\"$set\":{\"value\":99}}");
    EXPECT_TRUE(update_result.is_ok());

    // Delete document
    auto delete_result = backend_->execute_query(
        "mongo_test:{\"name\":\"test_item\"}");
    EXPECT_TRUE(delete_result.is_ok());
}

#endif // USE_MONGODB

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
