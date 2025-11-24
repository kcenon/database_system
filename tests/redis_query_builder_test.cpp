/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file redis_query_builder_test.cpp
 * @brief Unit tests for Redis Query Builder (DB-002)
 */

#include <gtest/gtest.h>
#include "database/query_builder.h"
#include <string>

namespace database::tests
{

class RedisQueryBuilderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        builder_ = std::make_unique<redis_query_builder>();
    }

    void TearDown() override
    {
        builder_.reset();
    }

    std::unique_ptr<redis_query_builder> builder_;
};

//=============================================================================
// String Operations Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Set)
{
    builder_->set("user:1:name", "John");
    auto query = builder_->build();

    EXPECT_EQ(query, "SET user:1:name John");
}

TEST_F(RedisQueryBuilderTest, Get)
{
    builder_->get("user:1:name");
    auto query = builder_->build();

    EXPECT_EQ(query, "GET user:1:name");
}

TEST_F(RedisQueryBuilderTest, Del)
{
    builder_->del("user:1:name");
    auto query = builder_->build();

    EXPECT_EQ(query, "DEL user:1:name");
}

TEST_F(RedisQueryBuilderTest, Exists)
{
    builder_->exists("user:1:name");
    auto query = builder_->build();

    EXPECT_EQ(query, "EXISTS user:1:name");
}

//=============================================================================
// Hash Operations Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Hset)
{
    builder_->hset("user:1", "name", "John");
    auto query = builder_->build();

    EXPECT_EQ(query, "HSET user:1 name John");
}

TEST_F(RedisQueryBuilderTest, Hget)
{
    builder_->hget("user:1", "name");
    auto query = builder_->build();

    EXPECT_EQ(query, "HGET user:1 name");
}

TEST_F(RedisQueryBuilderTest, Hdel)
{
    builder_->hdel("user:1", "name");
    auto query = builder_->build();

    EXPECT_EQ(query, "HDEL user:1 name");
}

TEST_F(RedisQueryBuilderTest, Hgetall)
{
    builder_->hgetall("user:1");
    auto query = builder_->build();

    EXPECT_EQ(query, "HGETALL user:1");
}

//=============================================================================
// List Operations Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Lpush)
{
    builder_->lpush("queue:tasks", "task1");
    auto query = builder_->build();

    EXPECT_EQ(query, "LPUSH queue:tasks task1");
}

TEST_F(RedisQueryBuilderTest, Rpush)
{
    builder_->rpush("queue:tasks", "task1");
    auto query = builder_->build();

    EXPECT_EQ(query, "RPUSH queue:tasks task1");
}

TEST_F(RedisQueryBuilderTest, Lpop)
{
    builder_->lpop("queue:tasks");
    auto query = builder_->build();

    EXPECT_EQ(query, "LPOP queue:tasks");
}

TEST_F(RedisQueryBuilderTest, Rpop)
{
    builder_->rpop("queue:tasks");
    auto query = builder_->build();

    EXPECT_EQ(query, "RPOP queue:tasks");
}

TEST_F(RedisQueryBuilderTest, Lrange)
{
    builder_->lrange("queue:tasks", 0, -1);
    auto query = builder_->build();

    EXPECT_EQ(query, "LRANGE queue:tasks 0 -1");
}

TEST_F(RedisQueryBuilderTest, LrangeWithRange)
{
    builder_->lrange("mylist", 0, 9);
    auto query = builder_->build();

    EXPECT_EQ(query, "LRANGE mylist 0 9");
}

//=============================================================================
// Set Operations Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Sadd)
{
    builder_->sadd("users:active", "user1");
    auto query = builder_->build();

    EXPECT_EQ(query, "SADD users:active user1");
}

TEST_F(RedisQueryBuilderTest, Srem)
{
    builder_->srem("users:active", "user1");
    auto query = builder_->build();

    EXPECT_EQ(query, "SREM users:active user1");
}

TEST_F(RedisQueryBuilderTest, Sismember)
{
    builder_->sismember("users:active", "user1");
    auto query = builder_->build();

    EXPECT_EQ(query, "SISMEMBER users:active user1");
}

TEST_F(RedisQueryBuilderTest, Smembers)
{
    builder_->smembers("users:active");
    auto query = builder_->build();

    EXPECT_EQ(query, "SMEMBERS users:active");
}

//=============================================================================
// Expiration Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Expire)
{
    builder_->expire("session:abc123", 3600);
    auto query = builder_->build();

    EXPECT_EQ(query, "EXPIRE session:abc123 3600");
}

TEST_F(RedisQueryBuilderTest, Ttl)
{
    builder_->ttl("session:abc123");
    auto query = builder_->build();

    EXPECT_EQ(query, "TTL session:abc123");
}

//=============================================================================
// Build Args Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, BuildArgsSet)
{
    builder_->set("key", "value");
    auto args = builder_->build_args();

    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "SET");
    EXPECT_EQ(args[1], "key");
    EXPECT_EQ(args[2], "value");
}

TEST_F(RedisQueryBuilderTest, BuildArgsGet)
{
    builder_->get("key");
    auto args = builder_->build_args();

    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "GET");
    EXPECT_EQ(args[1], "key");
}

TEST_F(RedisQueryBuilderTest, BuildArgsHset)
{
    builder_->hset("hash", "field", "value");
    auto args = builder_->build_args();

    ASSERT_EQ(args.size(), 4);
    EXPECT_EQ(args[0], "HSET");
    EXPECT_EQ(args[1], "hash");
    EXPECT_EQ(args[2], "field");
    EXPECT_EQ(args[3], "value");
}

TEST_F(RedisQueryBuilderTest, BuildArgsLrange)
{
    builder_->lrange("list", 0, 10);
    auto args = builder_->build_args();

    ASSERT_EQ(args.size(), 4);
    EXPECT_EQ(args[0], "LRANGE");
    EXPECT_EQ(args[1], "list");
    EXPECT_EQ(args[2], "0");
    EXPECT_EQ(args[3], "10");
}

//=============================================================================
// Reset & Reuse Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, Reset)
{
    builder_->set("key", "value");
    builder_->reset();

    auto query = builder_->build();
    EXPECT_TRUE(query.empty());
}

TEST_F(RedisQueryBuilderTest, ReuseAfterReset)
{
    builder_->set("key1", "value1");
    builder_->reset();

    builder_->get("key2");
    auto query = builder_->build();

    EXPECT_EQ(query, "GET key2");
    EXPECT_TRUE(query.find("key1") == std::string::npos);
}

//=============================================================================
// Key Naming Convention Tests
//=============================================================================

TEST_F(RedisQueryBuilderTest, ColonSeparatedKey)
{
    builder_->get("user:123:profile");
    auto query = builder_->build();

    EXPECT_EQ(query, "GET user:123:profile");
}

TEST_F(RedisQueryBuilderTest, NestedHashKey)
{
    builder_->hget("session:abc123", "user_id");
    auto query = builder_->build();

    EXPECT_EQ(query, "HGET session:abc123 user_id");
}

//=============================================================================
// Common Use Cases
//=============================================================================

TEST_F(RedisQueryBuilderTest, SessionStorage)
{
    // Store session
    builder_->set("session:token123", "user_data_json");
    auto set_query = builder_->build();
    EXPECT_EQ(set_query, "SET session:token123 user_data_json");

    // Get session
    builder_->reset();
    builder_->get("session:token123");
    auto get_query = builder_->build();
    EXPECT_EQ(get_query, "GET session:token123");
}

TEST_F(RedisQueryBuilderTest, UserProfileHash)
{
    // Set user profile fields
    builder_->hset("user:1", "name", "John");
    auto hset_query = builder_->build();
    EXPECT_EQ(hset_query, "HSET user:1 name John");

    // Get all user profile fields
    builder_->reset();
    builder_->hgetall("user:1");
    auto hgetall_query = builder_->build();
    EXPECT_EQ(hgetall_query, "HGETALL user:1");
}

TEST_F(RedisQueryBuilderTest, MessageQueue)
{
    // Push to queue
    builder_->rpush("queue:emails", "email_job_1");
    auto push_query = builder_->build();
    EXPECT_EQ(push_query, "RPUSH queue:emails email_job_1");

    // Pop from queue
    builder_->reset();
    builder_->lpop("queue:emails");
    auto pop_query = builder_->build();
    EXPECT_EQ(pop_query, "LPOP queue:emails");
}

TEST_F(RedisQueryBuilderTest, ActiveUsersSet)
{
    // Add user to active set
    builder_->sadd("users:online", "user123");
    auto add_query = builder_->build();
    EXPECT_EQ(add_query, "SADD users:online user123");

    // Check if user is active
    builder_->reset();
    builder_->sismember("users:online", "user123");
    auto check_query = builder_->build();
    EXPECT_EQ(check_query, "SISMEMBER users:online user123");

    // Remove user from active set
    builder_->reset();
    builder_->srem("users:online", "user123");
    auto remove_query = builder_->build();
    EXPECT_EQ(remove_query, "SREM users:online user123");
}

TEST_F(RedisQueryBuilderTest, SessionWithExpiration)
{
    // Set session with expiration
    builder_->set("session:abc", "session_data");
    auto set_query = builder_->build();
    EXPECT_EQ(set_query, "SET session:abc session_data");

    builder_->reset();
    builder_->expire("session:abc", 1800);  // 30 minutes
    auto expire_query = builder_->build();
    EXPECT_EQ(expire_query, "EXPIRE session:abc 1800");

    // Check TTL
    builder_->reset();
    builder_->ttl("session:abc");
    auto ttl_query = builder_->build();
    EXPECT_EQ(ttl_query, "TTL session:abc");
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(RedisQueryBuilderTest, EmptyKey)
{
    builder_->get("");
    auto query = builder_->build();

    EXPECT_EQ(query, "GET ");
}

TEST_F(RedisQueryBuilderTest, ValueWithSpaces)
{
    builder_->set("key", "value with spaces");
    auto query = builder_->build();

    EXPECT_EQ(query, "SET key value with spaces");
}

TEST_F(RedisQueryBuilderTest, NumericValues)
{
    builder_->expire("key", 0);
    auto query = builder_->build();

    EXPECT_EQ(query, "EXPIRE key 0");
}

TEST_F(RedisQueryBuilderTest, NegativeIndex)
{
    builder_->lrange("list", -10, -1);
    auto query = builder_->build();

    EXPECT_EQ(query, "LRANGE list -10 -1");
}

} // namespace database::tests
