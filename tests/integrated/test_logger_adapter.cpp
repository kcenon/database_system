// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_logger_adapter.cpp
 * @brief Unit tests for logger_adapter
 *
 * Tests both common_system logging integration and fallback modes.
 */

#include <kcenon/database/integrated/adapters/logger_adapter.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace kcenon::database::integrated;
using namespace kcenon::database::integrated::adapters;

namespace fs = std::filesystem;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message)                                        \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "  ✗ FAILED: " << message << "\n";                          \
      tests_failed++;                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_EXPECT_TRUE(condition, message) TEST_ASSERT(condition, message)
#define TEST_EXPECT_FALSE(condition, message) TEST_ASSERT(!(condition), message)

// ═══════════════════════════════════════════════════════════════
// Test Helper Functions
// ═══════════════════════════════════════════════════════════════

/**
 * @brief Clean up test log directory
 */
void cleanup_test_logs() {
  const std::string test_log_dir = "./test_logs";
  if (fs::exists(test_log_dir)) {
    fs::remove_all(test_log_dir);
  }
}

/**
 * @brief Check if a log file contains a specific string
 */
bool log_file_contains(const std::string &log_dir, const std::string &pattern) {
  if (!fs::exists(log_dir)) {
    return false;
  }

  for (const auto &entry : fs::directory_iterator(log_dir)) {
    if (entry.is_regular_file()) {
      std::ifstream file(entry.path());
      std::string line;
      while (std::getline(file, line)) {
        if (line.find(pattern) != std::string::npos) {
          return true;
        }
      }
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════
// Test Cases
// ═══════════════════════════════════════════════════════════════

/**
 * @brief Test logger initialization and shutdown
 */
bool test_initialization() {
  std::cout << "\nTesting logger initialization and shutdown...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);

  // Test initialization
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(),
                   "Logger should initialize successfully");
  TEST_EXPECT_TRUE(logger.is_initialized(),
                   "Logger should be marked as initialized");

  // Test double initialization (should be safe)
  auto reinit_result = logger.initialize();
  TEST_EXPECT_TRUE(reinit_result.is_ok(), "Re-initialization should be safe");

  // Test shutdown
  auto shutdown_result = logger.shutdown();
  TEST_EXPECT_TRUE(shutdown_result.is_ok(),
                   "Logger should shutdown successfully");
  TEST_EXPECT_FALSE(logger.is_initialized(),
                    "Logger should not be initialized after shutdown");

  // Test double shutdown (should be safe)
  auto reshutdown_result = logger.shutdown();
  TEST_EXPECT_TRUE(reshutdown_result.is_ok(), "Re-shutdown should be safe");

  std::cout << "  ✓ Initialization and shutdown tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test basic logging functionality
 */
bool test_basic_logging() {
  std::cout << "\nTesting basic logging...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Log at different levels
  logger.log(db_log_level::debug, "Debug message");
  logger.log(db_log_level::info, "Info message");
  logger.log(db_log_level::warning, "Warning message");
  logger.log(db_log_level::error, "Error message");

  // Flush to ensure logs are written
  logger.flush();

  // Give a small delay for async writes
  std::this_thread::yield();

  // Verify logs were written (if file logging is working)
  if (fs::exists("./test_logs")) {
    TEST_EXPECT_TRUE(log_file_contains("./test_logs", "Debug message") ||
                         log_file_contains("./test_logs", "Info message"),
                     "Log file should contain logged messages");
  }

  logger.shutdown();

  std::cout << "  ✓ Basic logging tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test query logging and SQL sanitization
 */
bool test_query_logging() {
  std::cout << "\nTesting query logging and SQL sanitization...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_query_logging = true;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Test normal query logging
  logger.log_query(db_log_level::info, "SELECT * FROM users WHERE id = 123",
                   std::chrono::microseconds(1500));

  // Test query with password (should be sanitized)
  logger.log_query(db_log_level::info,
                   "CREATE USER 'admin' IDENTIFIED BY PASSWORD 'secret123'",
                   std::chrono::microseconds(500));

  // Test very long query (should be truncated)
  std::string long_query = "SELECT ";
  for (int i = 0; i < 1000; i++) {
    long_query += "column" + std::to_string(i) + ", ";
  }
  long_query += "* FROM large_table";

  logger.log_query(db_log_level::info, long_query,
                   std::chrono::microseconds(2000));

  logger.flush();
  std::this_thread::yield();

  // Verify sanitization (password should not appear in logs)
  if (fs::exists("./test_logs")) {
    TEST_EXPECT_FALSE(log_file_contains("./test_logs", "secret123"),
                      "Password should be sanitized from logs");
  }

  logger.shutdown();

  std::cout << "  ✓ Query logging and sanitization tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test slow query detection
 */
bool test_slow_query_detection() {
  std::cout << "\nTesting slow query detection...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.log_slow_queries = true;
  config.slow_query_threshold = std::chrono::milliseconds(500);
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Test fast query (should not trigger slow query warning)
  logger.log_query(db_log_level::info, "SELECT * FROM fast_table",
                   std::chrono::microseconds(100000)); // 100ms

  // Test slow query (should trigger warning)
  logger.log_query(db_log_level::info, "SELECT * FROM slow_table",
                   std::chrono::microseconds(600000)); // 600ms

  // Explicit slow query log
  logger.log_slow_query("SELECT * FROM very_slow_table",
                        std::chrono::microseconds(1200000),
                        std::chrono::milliseconds(500));

  logger.flush();
  std::this_thread::yield();

  // Verify slow query warning was logged
  if (fs::exists("./test_logs")) {
    TEST_EXPECT_TRUE(log_file_contains("./test_logs", "SLOW QUERY") ||
                         log_file_contains("./test_logs", "slow_table"),
                     "Slow query should be detected and logged");
  }

  logger.shutdown();

  std::cout << "  ✓ Slow query detection tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test connection event logging
 */
bool test_connection_logging() {
  std::cout << "\nTesting connection event logging...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_connection_logging = true;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Test various connection events
  logger.log_connection_event("acquired", "Pool: main_pool, Priority: high");
  logger.log_connection_event("released", "Pool: main_pool, Connection ID: 42");
  logger.log_connection_event("timeout", "Pool: main_pool, Wait time: 30s");

  // Test pool events
  logger.log_pool_event("resized", 15, 5); // 15 active, 5 idle
  logger.log_pool_event("shrunk", 10, 8);  // 10 active, 8 idle
  logger.log_pool_event("health_check", 12, 6);

  logger.flush();
  std::this_thread::yield();

  logger.shutdown();

  std::cout << "  ✓ Connection event logging tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test transaction logging
 */
bool test_transaction_logging() {
  std::cout << "\nTesting transaction logging...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Test successful transactions
  logger.log_transaction("begin", true, "Isolation level: READ COMMITTED");
  logger.log_transaction("commit", true, "");

  // Test failed transaction
  logger.log_transaction("rollback", false,
                         "Constraint violation: duplicate key");

  logger.flush();
  std::this_thread::yield();

  logger.shutdown();

  std::cout << "  ✓ Transaction logging tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test error logging
 */
bool test_error_logging() {
  std::cout << "\nTesting error logging...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Test various errors
  logger.log_error("execute_query", "Connection lost", "08006");
  logger.log_error("connect", "Authentication failed", "28000");
  logger.log_error("prepare_statement", "Syntax error", "42601");

  logger.flush();
  std::this_thread::yield();

  logger.shutdown();

  std::cout << "  ✓ Error logging tests passed\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test thread safety (basic concurrency test)
 */
bool test_thread_safety() {
  std::cout << "\nTesting thread safety...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::debug;

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // Spawn multiple threads that log concurrently
  const int num_threads = 4;
  const int logs_per_thread = 50;
  std::vector<std::thread> threads;

  for (int t = 0; t < num_threads; t++) {
    threads.emplace_back([&logger, t, logs_per_thread]() {
      for (int i = 0; i < logs_per_thread; i++) {
        logger.log(db_log_level::info, "Thread " + std::to_string(t) +
                                           " message " + std::to_string(i));
      }
    });
  }

  // Wait for all threads to complete
  for (auto &thread : threads) {
    thread.join();
  }

  logger.flush();
  std::this_thread::yield();

  logger.shutdown();

  std::cout << "  ✓ Thread safety tests passed (no crashes)\n";
  tests_passed++;
  return true;
}

/**
 * @brief Test log level filtering
 */
bool test_log_level_filtering() {
  std::cout << "\nTesting log level filtering...\n";

  cleanup_test_logs();

  db_logger_config config;
  config.enable_file_logging = true;
  config.log_directory = "./test_logs";
  config.min_log_level = db_log_level::warning; // Only warning and above

  logger_adapter logger(config);
  auto init_result = logger.initialize();
  TEST_EXPECT_TRUE(init_result.is_ok(), "Logger should initialize");

  // These should be filtered out
  logger.log(db_log_level::trace, "Trace message - should not appear");
  logger.log(db_log_level::debug, "Debug message - should not appear");
  logger.log(db_log_level::info, "Info message - should not appear");

  // These should appear
  logger.log(db_log_level::warning, "Warning message - should appear");
  logger.log(db_log_level::error, "Error message - should appear");
  logger.log(db_log_level::critical, "Critical message - should appear");

  logger.flush();
  std::this_thread::yield();

  // Verify filtering
  if (fs::exists("./test_logs")) {
    TEST_EXPECT_FALSE(log_file_contains("./test_logs", "should not appear"),
                      "Debug/Info logs should be filtered out");
    TEST_EXPECT_TRUE(log_file_contains("./test_logs", "should appear") ||
                         log_file_contains("./test_logs", "Warning message"),
                     "Warning/Error logs should appear");
  }

  logger.shutdown();

  std::cout << "  ✓ Log level filtering tests passed\n";
  tests_passed++;
  return true;
}

// ═══════════════════════════════════════════════════════════════
// Main Test Runner
// ═══════════════════════════════════════════════════════════════

int main() {
  std::cout << "=== Running Logger Adapter Tests ===\n";
  std::cout << "\nMode: Backend pattern with runtime selection\n";
  std::cout
      << "Backend will be auto-selected (common_logger -> fallback_logger)\n";

  // Run all tests
  test_initialization();
  test_basic_logging();
  test_query_logging();
  test_slow_query_detection();
  test_connection_logging();
  test_transaction_logging();
  test_error_logging();
  test_thread_safety();
  test_log_level_filtering();

  // Cleanup
  cleanup_test_logs();

  // Print summary
  std::cout << "\n=== Test Summary ===\n";
  std::cout << "Passed: " << tests_passed << "\n";
  std::cout << "Failed: " << tests_failed << "\n";

  if (tests_failed == 0) {
    std::cout << "\n=== All tests passed! ✓ ===\n";
    return 0;
  } else {
    std::cout << "\n=== Some tests failed! ✗ ===\n";
    return 1;
  }
}
