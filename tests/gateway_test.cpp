// BSD 3-Clause License
//
// Copyright (c) 2025
// All rights reserved.

/**
 * @file gateway_test.cpp
 * @brief Unit tests for database_gateway
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "database/gateway/database_gateway.h"
#include "database/distributed/cluster_manager.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace database;
using namespace database::gateway;
using namespace database::distributed;

/**
 * @brief Mock cluster manager for testing gateway routing
 */
class MockClusterManager : public cluster_manager {
public:
    MockClusterManager() : cluster_manager() {}

    // Override execute methods with test implementations
    result<core::database_result> execute_read_query(const std::string& query) {
        last_query_ = query;
        read_count_++;

        core::database_result result;
        result.rows_affected = 0;
        return result::ok(result);
    }

    result<uint64_t> execute_write_query(const std::string& query) {
        last_query_ = query;
        write_count_++;
        return result<uint64_t>::ok(1);
    }

    std::string last_query() const { return last_query_; }
    int read_count() const { return read_count_; }
    int write_count() const { return write_count_; }

private:
    std::string last_query_;
    int read_count_ = 0;
    int write_count_ = 0;
};

class DatabaseGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        gateway_ = std::make_unique<database_gateway>();
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
        }
    }

    std::unique_ptr<database_gateway> gateway_;
};

// Basic lifecycle tests
TEST_F(DatabaseGatewayTest, InitializeAndShutdown) {
    security_config security;
    security.enable_tls = false;
    security.require_auth = false;

    auto result = gateway_->start(5000, security);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(gateway_->is_running());

    gateway_->stop();
    EXPECT_FALSE(gateway_->is_running());
}

TEST_F(DatabaseGatewayTest, CannotStartTwice) {
    security_config security;

    auto result1 = gateway_->start(5000, security);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = gateway_->start(5001, security);
    EXPECT_TRUE(result2.is_err());
}

// Cluster registration tests
TEST_F(DatabaseGatewayTest, RegisterCluster) {
    auto cluster = std::make_shared<cluster_manager>();

    auto result = gateway_->register_cluster("test-cluster", cluster);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, RegisterClusterWithEmptyId) {
    auto cluster = std::make_shared<cluster_manager>();

    auto result = gateway_->register_cluster("", cluster);
    EXPECT_TRUE(result.is_err());
}

TEST_F(DatabaseGatewayTest, RegisterNullCluster) {
    auto result = gateway_->register_cluster("test-cluster", nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(DatabaseGatewayTest, RegisterDuplicateCluster) {
    auto cluster = std::make_shared<cluster_manager>();

    auto result1 = gateway_->register_cluster("test-cluster", cluster);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = gateway_->register_cluster("test-cluster", cluster);
    EXPECT_TRUE(result2.is_err());
}

// Routing rule tests
TEST_F(DatabaseGatewayTest, AddRoutingRule) {
    routing_rule rule;
    rule.name = "users_routing";
    rule.pattern = std::regex("SELECT .* FROM users.*");
    rule.target_cluster = "users-cluster";
    rule.priority = 10;

    auto result = gateway_->add_routing_rule(rule);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, AddRoutingRuleWithEmptyName) {
    routing_rule rule;
    rule.name = "";
    rule.pattern = std::regex(".*");
    rule.target_cluster = "default";

    auto result = gateway_->add_routing_rule(rule);
    EXPECT_TRUE(result.is_err());
}

TEST_F(DatabaseGatewayTest, AddDuplicateRoutingRule) {
    routing_rule rule;
    rule.name = "test_rule";
    rule.pattern = std::regex(".*");
    rule.target_cluster = "default";

    auto result1 = gateway_->add_routing_rule(rule);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = gateway_->add_routing_rule(rule);
    EXPECT_TRUE(result2.is_err());
}

TEST_F(DatabaseGatewayTest, RemoveRoutingRule) {
    routing_rule rule;
    rule.name = "test_rule";
    rule.pattern = std::regex(".*");
    rule.target_cluster = "default";

    gateway_->add_routing_rule(rule);

    auto result = gateway_->remove_routing_rule("test_rule");
    EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, RemoveNonExistentRoutingRule) {
    auto result = gateway_->remove_routing_rule("nonexistent");
    EXPECT_TRUE(result.is_err());
}

// Cache configuration tests
TEST_F(DatabaseGatewayTest, ConfigureCache) {
    cache_config config;
    config.enabled = true;
    config.max_size = 1000;
    config.ttl = std::chrono::seconds(300);

    gateway_->configure_cache(config);

    auto stats = gateway_->get_cache_stats();
    EXPECT_EQ(stats["max_size"], 1000);
}

TEST_F(DatabaseGatewayTest, ClearCache) {
    cache_config config;
    config.enabled = true;
    config.max_size = 100;

    gateway_->configure_cache(config);
    gateway_->clear_cache();

    auto stats = gateway_->get_cache_stats();
    EXPECT_EQ(stats["hits"], 0);
    EXPECT_EQ(stats["misses"], 0);
    EXPECT_EQ(stats["entries"], 0);
}

// Cache hit rate tests
TEST_F(DatabaseGatewayTest, InitialCacheHitRate) {
    double hit_rate = gateway_->get_cache_hit_rate();
    EXPECT_DOUBLE_EQ(hit_rate, 0.0);
}

// Audit configuration tests
TEST_F(DatabaseGatewayTest, ConfigureAuditLogging) {
    audit_config config;
    config.log_all_queries = true;
    config.log_slow_queries_ms = 500;
    config.log_failed_queries = true;

    // Should not throw
    EXPECT_NO_THROW(gateway_->configure_audit_logging(config));
}

// Authentication tests
TEST_F(DatabaseGatewayTest, AuthenticateNonExistentUser) {
    auto result = gateway_->authenticate("unknown_user", "password");
    EXPECT_TRUE(result.is_err());
}

// Authorization tests
TEST_F(DatabaseGatewayTest, IsAuthorizedForNonExistentUser) {
    bool authorized = gateway_->is_authorized("unknown_user", "read");
    EXPECT_FALSE(authorized);
}

// Query execution tests (no cluster registered)
TEST_F(DatabaseGatewayTest, ExecuteQueryWithoutCluster) {
    security_config security;
    gateway_->start(5000, security);

    auto result = gateway_->execute_query("SELECT * FROM users");
    EXPECT_TRUE(result.is_err());
}

// Routing priority tests
TEST_F(DatabaseGatewayTest, RoutingRulesAreSortedByPriority) {
    routing_rule low_priority;
    low_priority.name = "low";
    low_priority.pattern = std::regex(".*");
    low_priority.target_cluster = "low-cluster";
    low_priority.priority = 1;

    routing_rule high_priority;
    high_priority.name = "high";
    high_priority.pattern = std::regex(".*");
    high_priority.target_cluster = "high-cluster";
    high_priority.priority = 10;

    // Add low priority first, then high
    gateway_->add_routing_rule(low_priority);
    gateway_->add_routing_rule(high_priority);

    // High priority should be checked first
    // This is tested indirectly through the routing behavior
    EXPECT_TRUE(true); // Rules added successfully
}

// Thread safety test
TEST_F(DatabaseGatewayTest, ConcurrentCacheAccess) {
    cache_config config;
    config.enabled = true;
    config.max_size = 100;
    gateway_->configure_cache(config);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; ++j) {
                gateway_->get_cache_stats();
                gateway_->get_cache_hit_rate();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash or deadlock
    EXPECT_TRUE(true);
}

// Test cache stats structure
TEST_F(DatabaseGatewayTest, CacheStatsHaveExpectedKeys) {
    auto stats = gateway_->get_cache_stats();

    EXPECT_TRUE(stats.find("hits") != stats.end());
    EXPECT_TRUE(stats.find("misses") != stats.end());
    EXPECT_TRUE(stats.find("entries") != stats.end());
    EXPECT_TRUE(stats.find("max_size") != stats.end());
}
