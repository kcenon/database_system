// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file test_database_coordinator.cpp
 * @brief Unit tests for database_coordinator (Phase 5)
 *
 * Tests the coordinator's ability to:
 * - Initialize and shutdown adapters in correct order
 * - Provide access to initialized adapters
 * - Handle initialization failures gracefully
 * - Perform aggregate health checks
 * - Report statistics
 */

#include "../../database/integrated/adapters/logger_adapter.h"
#include "../../database/integrated/adapters/monitoring_adapter.h"
#include "../../database/integrated/adapters/thread_adapter.h"
#include "../../database/integrated/core/database_coordinator.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace database::integrated;
using namespace database::integrated::adapters;

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    std::cout << "Running test: " << #name << " ... ";                         \
    try {                                                                      \
      test_##name();                                                           \
      std::cout << "PASSED\n";                                                 \
      tests_passed++;                                                          \
    } catch (const std::exception &e) {                                        \
      std::cout << "FAILED: " << e.what() << "\n";                             \
      tests_failed++;                                                          \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(condition)                                                 \
  if (!(condition)) {                                                          \
    throw std::runtime_error("Assertion failed: " #condition);                 \
  }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

// Test 1: Basic initialization and shutdown
TEST(basic_initialization_and_shutdown) {
  unified_db_config config;
  config.logger.min_log_level = db_log_level::info;
  // enable_console_logging does not exist
  config.logger.enable_file_logging = false;
  config.monitoring.enable_metrics = true;
  config.thread.thread_count = 2;

  database_coordinator coordinator(config);

  // Should not be initialized yet
  ASSERT_FALSE(coordinator.is_initialized());

  // Initialize
  auto init_result = coordinator.initialize();
  ASSERT_TRUE(init_result.is_ok());
  ASSERT_TRUE(coordinator.is_initialized());

  // Shutdown
  auto shutdown_result = coordinator.shutdown();
  ASSERT_TRUE(shutdown_result.is_ok());
  ASSERT_FALSE(coordinator.is_initialized());
}

// Test 2: Adapter access
TEST(adapter_access) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist
  config.monitoring.enable_metrics = true;
  config.thread.thread_count = 2;

  database_coordinator coordinator(config);

  // Before initialization, adapters should be nullptr
  ASSERT_TRUE(coordinator.get_logger() == nullptr);
  ASSERT_TRUE(coordinator.get_monitor() == nullptr);
  ASSERT_TRUE(coordinator.get_thread_pool() == nullptr);

  // Initialize
  auto init_result = coordinator.initialize();
  ASSERT_TRUE(init_result.is_ok());

  // After initialization, adapters should be accessible
  ASSERT_TRUE(coordinator.get_logger() != nullptr);
  ASSERT_TRUE(coordinator.get_monitor() != nullptr);
  ASSERT_TRUE(coordinator.get_thread_pool() != nullptr);

  // Shutdown
  coordinator.shutdown();
}

// Test 3: Logger functionality through coordinator
TEST(logger_functionality) {
  unified_db_config config;
  config.logger.min_log_level = db_log_level::debug;
  // config.logger.enable_console_logging does not exist

  database_coordinator coordinator(config);
  coordinator.initialize();

  auto *logger = coordinator.get_logger();
  ASSERT_TRUE(logger != nullptr);

  // Log various levels
  logger->log(db_log_level::debug, "Debug message from coordinator test");
  logger->log(db_log_level::info, "Info message from coordinator test");
  logger->log(db_log_level::warning, "Warning message from coordinator test");

  coordinator.shutdown();
}

// Test 4: Monitoring functionality through coordinator
TEST(monitoring_functionality) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist
  config.monitoring.enable_metrics = true;
  config.monitoring.enable_profiling = true;

  database_coordinator coordinator(config);
  coordinator.initialize();

  auto *monitor = coordinator.get_monitor();
  ASSERT_TRUE(monitor != nullptr);

  // Record some metrics
  monitor->record_connection_acquired();
  monitor->record_query_execution(std::chrono::microseconds(100), true);
  monitor->update_pool_stats(1, 5, 10);

  // Get metrics
  auto metrics_result = monitor->get_database_metrics();
  ASSERT_TRUE(metrics_result.is_ok());
  ASSERT_TRUE(metrics_result.value().active_connections == 1);

  coordinator.shutdown();
}

// Test 5: Thread pool functionality through coordinator
TEST(thread_pool_functionality) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist
  config.thread.thread_count = 2;

  database_coordinator coordinator(config);
  coordinator.initialize();

  auto *thread_pool = coordinator.get_thread_pool();
  ASSERT_TRUE(thread_pool != nullptr);

  // Submit a simple task
  std::atomic<bool> task_executed{false};
  auto future = thread_pool->submit([&task_executed]() {
    task_executed = true;
    return 42;
  });

  auto value = future.get();
  ASSERT_TRUE(value == 42);
  ASSERT_TRUE(task_executed);

  coordinator.shutdown();
}

// Test 6: Health check
TEST(health_check) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist
  config.monitoring.enable_health_checks = true;
  config.thread.thread_count = 2;

  database_coordinator coordinator(config);

  // Health check before initialization should fail
  auto health_before = coordinator.check_health();
  ASSERT_FALSE(health_before.is_ok());

  // Initialize
  coordinator.initialize();

  // Health check after initialization should succeed
  auto health_after = coordinator.check_health();
  ASSERT_TRUE(health_after.is_ok());
  ASSERT_TRUE(health_after.value()); // Should be healthy

  coordinator.shutdown();
}

// Test 7: Statistics
TEST(statistics) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist

  database_coordinator coordinator(config);

  // Get stats before initialization
  auto stats_before = coordinator.get_stats();
  ASSERT_TRUE(stats_before.is_ok());
  ASSERT_FALSE(stats_before.value().is_initialized);
  ASSERT_TRUE(stats_before.value().uptime.count() == 0);

  // Initialize
  coordinator.initialize();
  std::this_thread::yield();

  // Get stats after initialization
  auto stats_after = coordinator.get_stats();
  ASSERT_TRUE(stats_after.is_ok());
  ASSERT_TRUE(stats_after.value().is_initialized);
  ASSERT_TRUE(stats_after.value().logger_healthy);
  ASSERT_TRUE(stats_after.value().monitoring_healthy);
  ASSERT_TRUE(stats_after.value().thread_pool_healthy);
  ASSERT_TRUE(stats_after.value().uptime.count() > 0);

  coordinator.shutdown();
}

// Test 8: Double initialization
TEST(double_initialization) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist

  database_coordinator coordinator(config);

  // First initialization should succeed
  auto init1 = coordinator.initialize();
  ASSERT_TRUE(init1.is_ok());

  // Second initialization should fail
  auto init2 = coordinator.initialize();
  ASSERT_FALSE(init2.is_ok());

  coordinator.shutdown();
}

// Test 9: Shutdown without initialization
TEST(shutdown_without_initialization) {
  unified_db_config config;
  database_coordinator coordinator(config);

  // Shutdown without initialize should succeed (no-op)
  auto result = coordinator.shutdown();
  ASSERT_TRUE(result.is_ok());
}

// Test 10: Automatic shutdown in destructor
TEST(automatic_shutdown_in_destructor) {
  unified_db_config config;
  // config.logger.enable_console_logging does not exist

  {
    database_coordinator coordinator(config);
    coordinator.initialize();
    // Destructor will call shutdown automatically
  }

  // If we get here without crash, test passed
  ASSERT_TRUE(true);
}

// Test 11: Integration - all adapters working together
TEST(full_integration) {
  unified_db_config config;
  config.logger.min_log_level = db_log_level::info;
  // config.logger.enable_console_logging does not exist
  config.monitoring.enable_metrics = true;
  config.monitoring.enable_profiling = true;
  config.thread.thread_count = 4;

  database_coordinator coordinator(config);
  coordinator.initialize();

  // Get all adapters
  auto *logger = coordinator.get_logger();
  auto *monitor = coordinator.get_monitor();
  auto *thread_pool = coordinator.get_thread_pool();

  ASSERT_TRUE(logger != nullptr);
  ASSERT_TRUE(monitor != nullptr);
  ASSERT_TRUE(thread_pool != nullptr);

  // Simulate database operations
  logger->log(db_log_level::info, "Starting database operations");

  // Submit async query simulation
  auto query_future = thread_pool->submit([&]() {
    // Yield to allow simulated query execution
    std::this_thread::yield();

    // Record metrics
    monitor->record_query_execution(std::chrono::microseconds(100), true);

    logger->log(db_log_level::debug, "Query executed successfully");

    return true;
  });

  ASSERT_TRUE(query_future.get() == true);

  // Check health
  auto health = coordinator.check_health();
  ASSERT_TRUE(health.is_ok());
  ASSERT_TRUE(health.value());

  // Get metrics
  auto metrics = monitor->get_database_metrics();
  ASSERT_TRUE(metrics.is_ok());
  ASSERT_TRUE(metrics.value().total_queries == 1);
  ASSERT_TRUE(metrics.value().successful_queries == 1);

  logger->log(db_log_level::info, "All operations completed");

  coordinator.shutdown();
}

// Main test runner
int main() {
  std::cout << "=== Database Coordinator Tests (Phase 5) ===\n\n";

  RUN_TEST(basic_initialization_and_shutdown);
  RUN_TEST(adapter_access);
  RUN_TEST(logger_functionality);
  RUN_TEST(monitoring_functionality);
  RUN_TEST(thread_pool_functionality);
  RUN_TEST(health_check);
  RUN_TEST(statistics);
  RUN_TEST(double_initialization);
  RUN_TEST(shutdown_without_initialization);
  RUN_TEST(automatic_shutdown_in_destructor);
  RUN_TEST(full_integration);

  std::cout << "\n=== Test Summary ===\n";
  std::cout << "Passed: " << tests_passed << "\n";
  std::cout << "Failed: " << tests_failed << "\n";

  if (tests_failed == 0) {
    std::cout << "=== All tests passed! ✓ ===\n";
    return 0;
  } else {
    std::cout << "=== Some tests failed! ✗ ===\n";
    return 1;
  }
}
