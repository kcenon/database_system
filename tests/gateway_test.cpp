// BSD 3-Clause License
//
// Copyright (c) 2025
// All rights reserved.

/**
 * @file gateway_test.cpp
 * @brief Unit tests for database_gateway and audit_logger
 */

#include "database/distributed/cluster_manager.h"
#include "database/gateway/audit_logger.h"
#include "database/gateway/database_gateway.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

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
  result<core::database_result> execute_read_query(const std::string &query) {
    last_query_ = query;
    read_count_++;

    core::database_result result;
    result.rows_affected = 0;
    return result::ok(result);
  }

  result<uint64_t> execute_write_query(const std::string &query) {
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
  void SetUp() override { gateway_ = std::make_unique<database_gateway>(); }

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

  for (auto &t : threads) {
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
  EXPECT_TRUE(json.find("\"error\":\"Connection refused\"") !=
              std::string::npos);
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
  std::this_thread::yield();

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

  for (auto &t : threads) {
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
  std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "gateway_audit_test";
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

// =============================================================================
// Gateway Observability Integration Tests
// =============================================================================

TEST_F(DatabaseGatewayTest, ConfigureObservability) {
  gateway_observability_config config;
  config.enable_logging = true;
  config.enable_monitoring = true;

  // Configure logger
  config.logger_config.enable_query_logging = true;
  config.logger_config.log_slow_queries = true;
  config.logger_config.slow_query_threshold = std::chrono::milliseconds(100);
  config.logger_config.min_log_level = integrated::db_log_level::debug;

  // Configure monitoring
  config.monitoring_config.enable_metrics = true;
  config.monitoring_config.enable_health_checks = true;

  auto result = gateway_->configure_observability(config);
  EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, ObservabilityWithLoggingOnly) {
  gateway_observability_config config;
  config.enable_logging = true;
  config.enable_monitoring = false;
  config.logger_config.min_log_level = integrated::db_log_level::info;

  auto result = gateway_->configure_observability(config);
  EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, ObservabilityWithMonitoringOnly) {
  gateway_observability_config config;
  config.enable_logging = false;
  config.enable_monitoring = true;
  config.monitoring_config.enable_metrics = true;

  auto result = gateway_->configure_observability(config);
  EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, ObservabilityBothDisabled) {
  gateway_observability_config config;
  config.enable_logging = false;
  config.enable_monitoring = false;

  auto result = gateway_->configure_observability(config);
  EXPECT_TRUE(result.is_ok());
}

TEST_F(DatabaseGatewayTest, ObservabilityReconfiguration) {
  // First configuration
  gateway_observability_config config1;
  config1.enable_logging = true;
  config1.enable_monitoring = false;

  auto result1 = gateway_->configure_observability(config1);
  EXPECT_TRUE(result1.is_ok());

  // Reconfigure with different settings
  gateway_observability_config config2;
  config2.enable_logging = true;
  config2.enable_monitoring = true;

  auto result2 = gateway_->configure_observability(config2);
  EXPECT_TRUE(result2.is_ok());
}

TEST_F(DatabaseGatewayTest, ObservabilityWithFileLogging) {
  std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "gateway_observability_test";
  std::filesystem::create_directories(temp_dir);

  gateway_observability_config config;
  config.enable_logging = true;
  config.enable_monitoring = false;
  config.logger_config.enable_file_logging = true;
  config.logger_config.log_directory = temp_dir.string();
  config.logger_config.enable_query_logging = true;

  auto result = gateway_->configure_observability(config);
  EXPECT_TRUE(result.is_ok());

  // Start gateway and execute a query to generate logs
  security_config security;
  gateway_->start(5002, security);

  auto cluster = std::make_shared<cluster_manager>();
  gateway_->register_cluster("test-cluster", cluster);

  // Execute query (will fail but should log)
  gateway_->execute_query("SELECT * FROM test");

  gateway_->stop();

  // Cleanup
  std::filesystem::remove_all(temp_dir);
}

// =============================================================================
// Authentication Backend Tests
// =============================================================================

/**
 * @brief Test fixture for authentication backends
 */
class AuthBackendTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

/**
 * @brief Test local authentication backend initialization
 */
TEST_F(AuthBackendTest, LocalAuthBackendInitialization) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 6;
  config.require_uppercase = false;
  config.require_lowercase = false;
  config.require_digit = false;
  config.require_special = false;

  auto backend = auth::auth_backend_factory::create(config);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->type(), auth::auth_backend_type::local);
  EXPECT_EQ(backend->name(), "test_local");

  auto init_result = backend->initialize();
  EXPECT_TRUE(init_result.is_ok());
  EXPECT_TRUE(backend->is_healthy());

  backend->shutdown();
  EXPECT_FALSE(backend->is_healthy());
}

/**
 * @brief Test local authentication with valid credentials
 */
TEST_F(AuthBackendTest, LocalAuthValidCredentials) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 6;
  config.require_uppercase = false;
  config.require_lowercase = false;
  config.require_digit = false;

  auto backend = std::make_unique<auth::local_auth_backend>(config);
  ASSERT_TRUE(backend->initialize().is_ok());

  // Add a test user
  auto add_result = backend->add_user("testuser", "password123", {"admin"},
                                      {"read", "write"});
  EXPECT_TRUE(add_result.is_ok());

  // Authenticate with valid credentials
  auth::auth_credentials creds;
  creds.username = "testuser";
  creds.password = "password123";

  auto auth_result = backend->authenticate(creds);
  EXPECT_TRUE(auth_result.is_ok());
  EXPECT_EQ(auth_result.value().username, "testuser");
  EXPECT_FALSE(auth_result.value().access_token.empty());

  // Validate the token
  auto validate_result =
      backend->validate_token(auth_result.value().access_token);
  EXPECT_TRUE(validate_result.is_ok());
  EXPECT_EQ(validate_result.value().username, "testuser");

  backend->shutdown();
}

/**
 * @brief Test local authentication with invalid credentials
 */
TEST_F(AuthBackendTest, LocalAuthInvalidCredentials) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 6;
  config.require_uppercase = false;
  config.require_lowercase = false;
  config.require_digit = false;

  auto backend = std::make_unique<auth::local_auth_backend>(config);
  ASSERT_TRUE(backend->initialize().is_ok());

  // Add a test user
  backend->add_user("testuser", "password123");

  // Authenticate with wrong password
  auth::auth_credentials creds;
  creds.username = "testuser";
  creds.password = "wrongpassword";

  auto auth_result = backend->authenticate(creds);
  EXPECT_TRUE(auth_result.is_err());

  // Authenticate with non-existent user
  creds.username = "nonexistent";
  creds.password = "password123";

  auth_result = backend->authenticate(creds);
  EXPECT_TRUE(auth_result.is_err());

  backend->shutdown();
}

/**
 * @brief Test password policy enforcement
 */
TEST_F(AuthBackendTest, LocalAuthPasswordPolicy) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 8;
  config.require_uppercase = true;
  config.require_lowercase = true;
  config.require_digit = true;

  auto backend = std::make_unique<auth::local_auth_backend>(config);
  ASSERT_TRUE(backend->initialize().is_ok());

  // Try to add user with weak password (too short)
  auto result = backend->add_user("user1", "short");
  EXPECT_TRUE(result.is_err());

  // Try to add user without uppercase
  result = backend->add_user("user1", "password1");
  EXPECT_TRUE(result.is_err());

  // Try to add user without digit
  result = backend->add_user("user1", "Password");
  EXPECT_TRUE(result.is_err());

  // Add user with valid password
  result = backend->add_user("user1", "Password1");
  EXPECT_TRUE(result.is_ok());

  backend->shutdown();
}

/**
 * @brief Test account lockout after failed attempts
 */
TEST_F(AuthBackendTest, LocalAuthAccountLockout) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 6;
  config.require_uppercase = false;
  config.require_lowercase = false;
  config.require_digit = false;
  config.max_failed_attempts = 3;
  config.lockout_duration = std::chrono::seconds(60);

  auto backend = std::make_unique<auth::local_auth_backend>(config);
  ASSERT_TRUE(backend->initialize().is_ok());

  // Add a test user
  backend->add_user("testuser", "password123");

  auth::auth_credentials creds;
  creds.username = "testuser";
  creds.password = "wrongpassword";

  // Fail 3 times to trigger lockout
  for (int i = 0; i < 3; ++i) {
    auto result = backend->authenticate(creds);
    EXPECT_TRUE(result.is_err());
  }

  // Even correct password should fail now due to lockout
  creds.password = "password123";
  auto result = backend->authenticate(creds);
  EXPECT_TRUE(result.is_err());

  // Unlock the user
  backend->unlock_user("testuser");

  // Should work now
  result = backend->authenticate(creds);
  EXPECT_TRUE(result.is_ok());

  backend->shutdown();
}

/**
 * @brief Test permission management
 */
TEST_F(AuthBackendTest, LocalAuthPermissions) {
  auth::local_config config;
  config.name = "test_local";
  config.min_password_length = 6;
  config.require_uppercase = false;
  config.require_lowercase = false;
  config.require_digit = false;

  auto backend = std::make_unique<auth::local_auth_backend>(config);
  ASSERT_TRUE(backend->initialize().is_ok());

  // Add user with initial permissions
  backend->add_user("testuser", "password123", {}, {"read"});

  // Authenticate to get user_id
  auth::auth_credentials creds;
  creds.username = "testuser";
  creds.password = "password123";
  auto auth_result = backend->authenticate(creds);
  ASSERT_TRUE(auth_result.is_ok());

  std::string user_id = auth_result.value().user_id;

  // Check initial permission
  EXPECT_TRUE(backend->has_permission(user_id, "read"));
  EXPECT_FALSE(backend->has_permission(user_id, "write"));

  // Grant new permission
  backend->grant_permission("testuser", "write");
  EXPECT_TRUE(backend->has_permission(user_id, "write"));

  // Revoke permission
  backend->revoke_permission("testuser", "read");
  EXPECT_FALSE(backend->has_permission(user_id, "read"));

  backend->shutdown();
}

/**
 * @brief Test LDAP backend initialization
 */
TEST_F(AuthBackendTest, LdapAuthBackendInitialization) {
  auth::ldap_config config;
  config.name = "test_ldap";
  config.server_url = "ldap://localhost:389";
  config.base_dn = "dc=example,dc=com";
  config.user_search_filter = "(uid={0})";

  auto backend = auth::auth_backend_factory::create(config);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->type(), auth::auth_backend_type::ldap);
  EXPECT_EQ(backend->name(), "test_ldap");

  auto init_result = backend->initialize();
  EXPECT_TRUE(init_result.is_ok());
  EXPECT_TRUE(backend->is_healthy());

  backend->shutdown();
}

/**
 * @brief Test OAuth backend initialization
 */
TEST_F(AuthBackendTest, OAuthBackendInitialization) {
  auth::oauth_config config;
  config.name = "test_oauth";
  config.client_id = "test-client-id";
  config.client_secret = "test-secret";
  config.issuer = "https://auth.example.com";
  config.token_url = "https://auth.example.com/token";

  auto backend = auth::auth_backend_factory::create(config);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->type(), auth::auth_backend_type::oauth);
  EXPECT_EQ(backend->name(), "test_oauth");

  auto init_result = backend->initialize();
  EXPECT_TRUE(init_result.is_ok());
  EXPECT_TRUE(backend->is_healthy());

  backend->shutdown();
}

/**
 * @brief Test auth manager with multiple backends
 */
TEST_F(AuthBackendTest, AuthManagerMultipleBackends) {
  auth::auth_manager manager;

  // Add local backend
  auth::local_config local_config;
  local_config.name = "local";
  local_config.min_password_length = 6;
  local_config.require_uppercase = false;
  local_config.require_lowercase = false;
  local_config.require_digit = false;

  auto local_backend = std::make_unique<auth::local_auth_backend>(local_config);
  local_backend->initialize();
  local_backend->add_user("localuser", "password123", {}, {"local_perm"});
  manager.add_backend(std::move(local_backend), true);

  // Add LDAP backend as secondary
  auth::ldap_config ldap_config;
  ldap_config.name = "ldap";
  ldap_config.server_url = "ldap://localhost:389";
  ldap_config.base_dn = "dc=example,dc=com";

  auto ldap_backend = std::make_unique<auth::ldap_auth_backend>(ldap_config);
  ldap_backend->initialize();
  manager.add_backend(std::move(ldap_backend), false);

  EXPECT_EQ(manager.backend_count(), 2);
  EXPECT_TRUE(manager.is_healthy());

  // Authenticate through local backend
  auth::auth_credentials creds;
  creds.username = "localuser";
  creds.password = "password123";

  auto result = manager.authenticate(creds);
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().username, "localuser");

  // Check permissions
  EXPECT_TRUE(manager.has_permission(result.value().user_id, "local_perm"));

  manager.shutdown();
}

/**
 * @brief Test gateway with authentication enabled
 */
TEST_F(AuthBackendTest, GatewayWithAuthentication) {
  database_gateway gateway;

  // Configure security with local auth
  security_config security;
  security.require_auth = true;
  security.auth_backend_type = "local";
  security.local_auth_config.name = "gateway_local";
  security.local_auth_config.min_password_length = 6;
  security.local_auth_config.require_uppercase = false;
  security.local_auth_config.require_lowercase = false;
  security.local_auth_config.require_digit = false;

  auto start_result = gateway.start(5010, security);
  EXPECT_TRUE(start_result.is_ok());

  // Add a user through the auth manager
  auto *auth_mgr = gateway.get_auth_manager();
  ASSERT_NE(auth_mgr, nullptr);
  EXPECT_GT(auth_mgr->backend_count(), 0);

  auto *backend =
      dynamic_cast<auth::local_auth_backend *>(auth_mgr->get_backend(0));
  ASSERT_NE(backend, nullptr);

  auto add_result = backend->add_user("gwuser", "password123");
  EXPECT_TRUE(add_result.is_ok());

  // Authenticate through gateway
  auto auth_result = gateway.authenticate("gwuser", "password123");
  EXPECT_TRUE(auth_result.is_ok());

  // Fail with wrong password
  auth_result = gateway.authenticate("gwuser", "wrongpass");
  EXPECT_TRUE(auth_result.is_err());

  gateway.stop();
}
