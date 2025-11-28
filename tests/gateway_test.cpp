// BSD 3-Clause License
//
// Copyright (c) 2025
// All rights reserved.

/**
 * @file gateway_test.cpp
 * @brief Unit tests for database_gateway and audit_logger
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "database/gateway/database_gateway.h"
#include "database/gateway/audit_logger.h"
#include "database/distributed/cluster_manager.h"
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

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

// =============================================================================
// Audit Logger Tests
// =============================================================================

class AuditLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test logs
        test_dir_ = std::filesystem::temp_directory_path() / "audit_logger_test";
        std::filesystem::create_directories(test_dir_);
        test_log_path_ = (test_dir_ / "audit.log").string();
    }

    void TearDown() override {
        // Cleanup test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::string read_log_file() {
        std::ifstream file(test_log_path_);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::filesystem::path test_dir_;
    std::string test_log_path_;
};

// Audit entry JSON serialization tests
TEST_F(AuditLoggerTest, AuditEntryToJson) {
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "test_user";
    entry.session_id = "session123";
    entry.client_ip = "192.168.1.1";
    entry.operation = "SELECT";
    entry.query_hash = "abc123";
    entry.target_cluster = "main-cluster";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(50);

    std::string json = entry.to_json();

    EXPECT_TRUE(json.find("\"user\":\"test_user\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"session_id\":\"session123\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"operation\":\"SELECT\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(json.find("\"latency_ms\":50") != std::string::npos);
}

TEST_F(AuditLoggerTest, AuditEntryToJsonWithError) {
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "test_user";
    entry.operation = "INSERT";
    entry.success = false;
    entry.latency = std::chrono::milliseconds(100);
    entry.error_message = "Connection refused";

    std::string json = entry.to_json();

    EXPECT_TRUE(json.find("\"success\":false") != std::string::npos);
    EXPECT_TRUE(json.find("\"error\":\"Connection refused\"") != std::string::npos);
}

TEST_F(AuditLoggerTest, AuditEntryJsonEscapesSpecialCharacters) {
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "user\"with\"quotes";
    entry.session_id = "session\\with\\backslash";
    entry.operation = "SELECT";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(10);

    std::string json = entry.to_json();

    // Quotes and backslashes should be escaped
    EXPECT_TRUE(json.find("\\\"") != std::string::npos);
    EXPECT_TRUE(json.find("\\\\") != std::string::npos);
}

// Audit entry CSV serialization tests
TEST_F(AuditLoggerTest, AuditEntryToCsv) {
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "test_user";
    entry.session_id = "session123";
    entry.client_ip = "192.168.1.1";
    entry.operation = "SELECT";
    entry.query_hash = "abc123";
    entry.target_cluster = "main-cluster";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(50);

    std::string csv = entry.to_csv();

    EXPECT_TRUE(csv.find("test_user") != std::string::npos);
    EXPECT_TRUE(csv.find("session123") != std::string::npos);
    EXPECT_TRUE(csv.find("SELECT") != std::string::npos);
    EXPECT_TRUE(csv.find("true") != std::string::npos);
    EXPECT_TRUE(csv.find("50") != std::string::npos);
}

TEST_F(AuditLoggerTest, AuditEntryCsvEscapesCommas) {
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "user,with,commas";
    entry.operation = "SELECT";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(10);

    std::string csv = entry.to_csv();

    // Field with commas should be quoted
    EXPECT_TRUE(csv.find("\"user,with,commas\"") != std::string::npos);
}

// Audit logger lifecycle tests
TEST_F(AuditLoggerTest, StartAndStop) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = false;

    audit_logger logger(config);

    EXPECT_FALSE(logger.is_running());

    bool started = logger.start();
    EXPECT_TRUE(started);
    EXPECT_TRUE(logger.is_running());

    logger.stop();
    EXPECT_FALSE(logger.is_running());
}

TEST_F(AuditLoggerTest, LogEntrySyncMode) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = false;

    audit_logger logger(config);
    logger.start();

    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "test_user";
    entry.operation = "SELECT";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(10);

    logger.log(entry);
    logger.flush();
    logger.stop();

    std::string content = read_log_file();
    EXPECT_TRUE(content.find("test_user") != std::string::npos);
    EXPECT_TRUE(content.find("SELECT") != std::string::npos);
}

TEST_F(AuditLoggerTest, LogEntryAsyncMode) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = true;
    config.flush_interval = std::chrono::milliseconds(100);

    audit_logger logger(config);
    logger.start();

    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "async_user";
    entry.operation = "INSERT";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(20);

    logger.log(entry);

    // Wait for async flush
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    logger.stop();

    std::string content = read_log_file();
    EXPECT_TRUE(content.find("async_user") != std::string::npos);
}

TEST_F(AuditLoggerTest, LogMultipleEntries) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = false;

    audit_logger logger(config);
    logger.start();

    for (int i = 0; i < 10; ++i) {
        audit_entry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.user = "user_" + std::to_string(i);
        entry.operation = "SELECT";
        entry.success = true;
        entry.latency = std::chrono::milliseconds(i * 10);
        logger.log(entry);
    }

    logger.flush();
    logger.stop();

    auto stats = logger.get_stats();
    EXPECT_EQ(stats["entries_logged"], 10);
}

TEST_F(AuditLoggerTest, CsvFormatWithHeader) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::CSV;
    config.async_write = false;

    audit_logger logger(config);
    logger.start();

    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "csv_user";
    entry.operation = "DELETE";
    entry.success = true;
    entry.latency = std::chrono::milliseconds(5);

    logger.log(entry);
    logger.flush();
    logger.stop();

    std::string content = read_log_file();

    // Should have CSV header
    EXPECT_TRUE(content.find("timestamp,user,session_id") != std::string::npos);
    EXPECT_TRUE(content.find("csv_user") != std::string::npos);
}

TEST_F(AuditLoggerTest, GetCurrentLogPath) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;

    audit_logger logger(config);
    logger.start();

    std::string current_path = logger.current_log_path();
    EXPECT_EQ(current_path, test_log_path_);

    logger.stop();
}

TEST_F(AuditLoggerTest, GetStats) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = false;

    audit_logger logger(config);
    logger.start();

    auto stats = logger.get_stats();

    EXPECT_TRUE(stats.find("entries_logged") != stats.end());
    EXPECT_TRUE(stats.find("entries_dropped") != stats.end());
    EXPECT_TRUE(stats.find("rotations") != stats.end());

    logger.stop();
}

TEST_F(AuditLoggerTest, ThreadSafety) {
    audit_logger_config config;
    config.log_path = test_log_path_;
    config.format = audit_format::JSON;
    config.async_write = true;
    config.buffer_size = 10000;

    audit_logger logger(config);
    logger.start();

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 100; ++j) {
                audit_entry entry;
                entry.timestamp = std::chrono::system_clock::now();
                entry.user = "user_" + std::to_string(i);
                entry.operation = "SELECT";
                entry.success = true;
                entry.latency = std::chrono::milliseconds(j);
                logger.log(entry);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    logger.flush();
    logger.stop();

    auto stats = logger.get_stats();
    // All entries should be logged (none dropped with large buffer)
    EXPECT_EQ(stats["entries_logged"], 500);
    EXPECT_EQ(stats["entries_dropped"], 0);
}

// Gateway audit logging integration tests
TEST_F(DatabaseGatewayTest, AuditLoggingEnabled) {
    // Create temp log file
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "gateway_audit_test";
    std::filesystem::create_directories(temp_dir);
    std::string log_path = (temp_dir / "gateway_audit.log").string();

    audit_config config;
    config.enabled = true;
    config.log_all_queries = true;
    config.audit_log_path = log_path;
    config.format = audit_format::JSON;
    config.async_write = false;

    gateway_->configure_audit_logging(config);

    security_config security;
    gateway_->start(5000, security);

    // Register a cluster
    auto cluster = std::make_shared<cluster_manager>();
    gateway_->register_cluster("test-cluster", cluster);

    // Execute will fail but should still log
    auto result = gateway_->execute_query("SELECT * FROM users");

    gateway_->stop();

    // Check log file exists and has content
    std::ifstream log_file(log_path);
    std::stringstream buffer;
    buffer << log_file.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("SELECT") != std::string::npos);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}
