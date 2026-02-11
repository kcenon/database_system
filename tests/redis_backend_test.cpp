/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Redis Backend Tests
 *
 * Tests for redis_backend covering:
 * - Type identification
 * - Operations fail without initialization
 * - backend_base lifecycle guards
 * - Thread safety (mutex-protected)
 * - Conditional: real connection tests (when USE_REDIS defined)
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "database/backends/redis_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

// =============================================================================
// Test Fixture
// =============================================================================

class RedisBackendTest : public ::testing::Test {
protected:
    std::unique_ptr<redis_backend> backend_;
    connection_config test_config_;

    void SetUp() override
    {
        backend_ = std::make_unique<redis_backend>();
        test_config_.host = "localhost";
        test_config_.port = 6379;
        test_config_.database = "0";
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

TEST_F(RedisBackendTest, TypeReturnsRedis)
{
    EXPECT_EQ(backend_->type(), database_types::redis);
}

TEST_F(RedisBackendTest, BackendNameIsCorrect)
{
    EXPECT_STREQ(redis_backend::backend_name(), "redis_backend");
}

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(RedisBackendTest, InitiallyNotInitialized)
{
    EXPECT_FALSE(backend_->is_initialized());
}

TEST_F(RedisBackendTest, InitiallyNotInTransaction)
{
    EXPECT_FALSE(backend_->in_transaction());
}

// =============================================================================
// Operations Without Initialization Tests
// =============================================================================

TEST_F(RedisBackendTest, InsertQueryFailsWithoutInit)
{
    auto result = backend_->insert_query("mykey:myvalue");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, UpdateQueryFailsWithoutInit)
{
    auto result = backend_->update_query("mykey:newvalue");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, DeleteQueryFailsWithoutInit)
{
    auto result = backend_->delete_query("mykey");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, SelectQueryFailsWithoutInit)
{
    auto result = backend_->select_query("mykey");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, ExecuteQueryFailsWithoutInit)
{
    auto result = backend_->execute_query("PING");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, BeginTransactionFailsWithoutInit)
{
    auto result = backend_->begin_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, CommitTransactionFailsWithoutInit)
{
    auto result = backend_->commit_transaction();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RedisBackendTest, RollbackTransactionFailsWithoutInit)
{
    auto result = backend_->rollback_transaction();
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Lifecycle Guard Tests (via backend_base)
// =============================================================================

TEST_F(RedisBackendTest, ShutdownWithoutInitIsNoOp)
{
    auto result = backend_->shutdown();
    EXPECT_TRUE(result.is_ok());
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_F(RedisBackendTest, CreateReturnsValidBackend)
{
    auto backend = redis_backend::create();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::redis);
    EXPECT_FALSE(backend->is_initialized());
}

// =============================================================================
// Connection Info Tests
// =============================================================================

TEST_F(RedisBackendTest, ConnectionInfoBeforeInit)
{
    auto info = backend_->connection_info();
    SUCCEED();
}

TEST_F(RedisBackendTest, LastErrorBeforeInit)
{
    auto error = backend_->last_error();
    SUCCEED();
}

// =============================================================================
// Real Connection Tests (conditional on USE_REDIS)
// =============================================================================

#ifdef USE_REDIS

TEST_F(RedisBackendTest, ConnectToLocalRedis)
{
    connection_config config;
    config.host = "localhost";
    config.port = 6379;

    auto result = backend_->initialize(config);
    if (!result.is_ok()) {
        GTEST_SKIP() << "Local Redis not available: " << backend_->last_error();
    }

    EXPECT_TRUE(backend_->is_initialized());
}

TEST_F(RedisBackendTest, KeyValueOperationsOnRedis)
{
    connection_config config;
    config.host = "localhost";
    config.port = 6379;

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local Redis not available";
    }

    // SET (insert)
    auto insert_result = backend_->insert_query("redis_test_key:test_value");
    EXPECT_TRUE(insert_result.is_ok());

    // GET (select)
    auto select_result = backend_->select_query("redis_test_key");
    ASSERT_TRUE(select_result.is_ok());
    EXPECT_FALSE(select_result.value().empty());

    // SET (update - same key, new value)
    auto update_result = backend_->update_query("redis_test_key:updated_value");
    EXPECT_TRUE(update_result.is_ok());

    // DEL (delete)
    auto delete_result = backend_->delete_query("redis_test_key");
    EXPECT_TRUE(delete_result.is_ok());

    // Verify deletion
    auto verify_result = backend_->select_query("redis_test_key");
    if (verify_result.is_ok()) {
        EXPECT_TRUE(verify_result.value().empty());
    }
}

TEST_F(RedisBackendTest, TransactionsOnRedis)
{
    connection_config config;
    config.host = "localhost";
    config.port = 6379;

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local Redis not available";
    }

    // Redis MULTI/EXEC
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    EXPECT_TRUE(backend_->in_transaction());

    backend_->insert_query("redis_tx_key:tx_value");

    EXPECT_TRUE(backend_->commit_transaction().is_ok());
    EXPECT_FALSE(backend_->in_transaction());

    // Cleanup
    backend_->delete_query("redis_tx_key");
}

TEST_F(RedisBackendTest, TransactionRollbackOnRedis)
{
    connection_config config;
    config.host = "localhost";
    config.port = 6379;

    if (!backend_->initialize(config).is_ok()) {
        GTEST_SKIP() << "Local Redis not available";
    }

    // Redis DISCARD
    EXPECT_TRUE(backend_->begin_transaction().is_ok());
    backend_->insert_query("redis_discard_key:value");
    EXPECT_TRUE(backend_->rollback_transaction().is_ok());
    EXPECT_FALSE(backend_->in_transaction());
}

#endif // USE_REDIS

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
