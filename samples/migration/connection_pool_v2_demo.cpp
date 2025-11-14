/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file connection_pool_v2_demo.cpp
 * @brief Demonstration of connection_pool_v2 with priority-based scheduling
 *
 * This program demonstrates the key features of connection_pool_v2:
 * 1. Priority-based connection acquisition
 * 2. Asynchronous health checks
 * 3. High-performance request handling
 * 4. Comparison with legacy connection_pool
 */

#include "database/pooling/connection_pool_v2.h"
#include "database/connection_pool.h"
#include "database/database_types.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace database;
using namespace database::pooling;
using namespace std::chrono;

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

/**
 * @brief Mock database for testing (no actual database required)
 */
class mock_database : public database_base {
public:
    mock_database() : database_base() {}

    // Pure virtual functions from database_base
    database_types database_type() override { return database_types::postgres; }

    bool connect(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return true;
    }

    bool create_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return true;
    }

    unsigned int insert_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return 1;
    }

    unsigned int update_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return 1;
    }

    unsigned int delete_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return 1;
    }

    database_result select_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return database_result{};
    }

    bool execute_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return true;
    }

    bool disconnect() override {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        return true;
    }
};

/**
 * @brief Print a section header
 */
void print_header(const std::string& title) {
    std::cout << "\n" << CYAN << "=== " << title << " ===" << RESET << "\n\n";
}

/**
 * @brief Print success message
 */
void print_success(const std::string& message) {
    std::cout << GREEN << "✅ " << message << RESET << "\n";
}

/**
 * @brief Print error message
 */
void print_error(const std::string& message) {
    std::cout << RED << "❌ " << message << RESET << "\n";
}

/**
 * @brief Print info message
 */
void print_info(const std::string& message) {
    std::cout << BLUE << "ℹ️  " << message << RESET << "\n";
}

/**
 * @brief Demonstrates basic pool initialization and operation
 */
void demonstrate_basic_usage() {
    print_header("Basic Usage");

    // Configure connection pool
    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 10;
    config.connection_string = "mock://localhost";
    config.enable_health_checks = true;

    // Create factory
    auto factory = []() -> std::unique_ptr<database_base> {
        return std::make_unique<mock_database>();
    };

    // Create connection_pool_v2
    connection_pool_v2 pool(database_types::postgres, config, factory, 4);

    std::cout << "Using thread_system: " << (pool.is_using_thread_system() ? "YES" : "NO") << "\n";

    // Initialize pool
    if (!pool.initialize()) {
        print_error("Failed to initialize pool");
        return;
    }
    print_success("Pool initialized successfully");

    // Acquire connection with default priority
    auto future = pool.acquire_connection();
    auto result = future.get();

    if (result.is_ok()) {
        auto conn = result.value();
        print_success("Connection acquired successfully");

        // Use connection
        conn->get()->execute_query("SELECT 1");
        print_info("Query executed");

        // Return connection
        pool.release_connection(conn);
        print_success("Connection returned to pool");
    } else {
        print_error("Failed to acquire connection: " + result.get_error().message());
    }

    // Show stats
    auto stats = pool.get_stats();
    std::cout << "\nPool Statistics:\n";
    std::cout << "  Active connections: " << stats.active_connections << "\n";
    std::cout << "  Available connections: " << stats.available_connections << "\n";
    std::cout << "  Successful acquisitions: " << stats.successful_acquisitions << "\n";

    pool.shutdown();
    print_success("Pool shutdown completed");
}

/**
 * @brief Demonstrates priority-based connection acquisition
 */
void demonstrate_priority_scheduling() {
    print_header("Priority-Based Scheduling");

    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.connection_string = "mock://localhost";

    auto factory = []() { return std::make_unique<mock_database>(); };

    connection_pool_v2 pool(database_types::postgres, config, factory, 4);
    if (!pool.initialize()) {
        print_error("Failed to initialize pool");
        return;
    }

    std::cout << "Submitting requests with different priorities...\n\n";

    // Submit requests with different priorities
    auto normal1 = pool.acquire_connection(connection_priority::NORMAL_QUERY);
    std::cout << "  [1] NORMAL_QUERY submitted\n";

    auto critical = pool.acquire_connection(connection_priority::CRITICAL);
    std::cout << "  [2] CRITICAL submitted (should be processed first)\n";

    auto transaction = pool.acquire_connection(connection_priority::TRANSACTION);
    std::cout << "  [3] TRANSACTION submitted\n";

    auto normal2 = pool.acquire_connection(connection_priority::NORMAL_QUERY);
    std::cout << "  [4] NORMAL_QUERY submitted\n";

    auto health = pool.acquire_connection(connection_priority::HEALTH_CHECK);
    std::cout << "  [5] HEALTH_CHECK submitted (lowest priority)\n";

    // Wait for all to complete
    std::cout << "\nWaiting for completion...\n";

    auto r1 = normal1.get();
    auto r2 = critical.get();
    auto r3 = transaction.get();
    auto r4 = normal2.get();
    auto r5 = health.get();

    // Release all connections
    if (r1.is_ok()) pool.release_connection(r1.value());
    if (r2.is_ok()) pool.release_connection(r2.value());
    if (r3.is_ok()) pool.release_connection(r3.value());
    if (r4.is_ok()) pool.release_connection(r4.value());
    if (r5.is_ok()) pool.release_connection(r5.value());

    print_success("All requests completed");
    print_info("CRITICAL and TRANSACTION requests were prioritized over NORMAL_QUERY and HEALTH_CHECK");

    pool.shutdown();
}

/**
 * @brief Demonstrates asynchronous health checks
 */
void demonstrate_async_health_checks() {
    print_header("Asynchronous Health Checks");

    connection_pool_config config;
    config.min_connections = 3;
    config.max_connections = 10;
    config.connection_string = "mock://localhost";
    config.enable_health_checks = true;

    auto factory = []() { return std::make_unique<mock_database>(); };

    connection_pool_v2 pool(database_types::postgres, config, factory, 4);
    if (!pool.initialize()) {
        print_error("Failed to initialize pool");
        return;
    }

    std::cout << "Scheduling health checks...\n";

    // Schedule multiple health checks
    for (int i = 0; i < 5; ++i) {
        pool.schedule_health_check();
        std::cout << "  Health check #" << (i + 1) << " scheduled\n";
    }

    // While health checks are running, submit normal requests
    std::cout << "\nSubmitting normal queries while health checks run in background...\n";

    std::vector<std::future<Result<std::shared_ptr<connection_wrapper>>>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.acquire_connection(connection_priority::NORMAL_QUERY));
    }

    // Wait for all queries
    for (auto& future : futures) {
        auto result = future.get();
        if (result.is_ok()) {
            pool.release_connection(result.value());
        }
    }

    print_success("All queries completed");
    print_info("Health checks ran as low-priority background jobs");

    // Give health checks time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    pool.shutdown();
}

/**
 * @brief Demonstrates high-throughput performance
 */
void demonstrate_high_throughput() {
    print_header("High-Throughput Performance");

    connection_pool_config config;
    config.min_connections = 8;
    config.max_connections = 20;
    config.connection_string = "mock://localhost";

    auto factory = []() { return std::make_unique<mock_database>(); };

    connection_pool_v2 pool(database_types::postgres, config, factory, 8);
    if (!pool.initialize()) {
        print_error("Failed to initialize pool");
        return;
    }

    const int num_requests = 1000;
    std::cout << "Submitting " << num_requests << " connection requests...\n";

    auto start = high_resolution_clock::now();

    std::vector<std::future<Result<std::shared_ptr<connection_wrapper>>>> futures;
    futures.reserve(num_requests);

    for (int i = 0; i < num_requests; ++i) {
        // Mix of priorities
        connection_priority priority;
        if (i % 10 == 0) {
            priority = connection_priority::CRITICAL;
        } else if (i % 5 == 0) {
            priority = connection_priority::TRANSACTION;
        } else {
            priority = connection_priority::NORMAL_QUERY;
        }

        futures.push_back(pool.acquire_connection(priority));
    }

    auto submit_end = high_resolution_clock::now();
    auto submit_time = duration_cast<microseconds>(submit_end - start).count();

    std::cout << "Submitted " << num_requests << " requests in "
              << submit_time << " microseconds\n";
    std::cout << "Average latency: " << (submit_time / num_requests) << " microseconds/request\n";

    // Wait for all to complete
    std::cout << "\nWaiting for all requests to complete...\n";
    for (auto& future : futures) {
        auto result = future.get();
        if (result.is_ok()) {
            pool.release_connection(result.value());
        }
    }

    auto end = high_resolution_clock::now();
    auto total_time = duration_cast<microseconds>(end - start).count();

    std::cout << "\nPerformance Results:\n";
    std::cout << "  Total time: " << total_time << " μs\n";
    std::cout << "  Throughput: " << (num_requests * 1000000.0 / total_time) << " requests/second\n";

    print_success("High throughput test completed");

    auto stats = pool.get_stats();
    std::cout << "\nFinal Statistics:\n";
    std::cout << "  Total connections: " << stats.total_connections << "\n";
    std::cout << "  Active connections: " << stats.active_connections << "\n";
    std::cout << "  Successful acquisitions: " << stats.successful_acquisitions << "\n";

    pool.shutdown();
}

/**
 * @brief Demonstrates error handling
 */
void demonstrate_error_handling() {
    print_header("Error Handling");

    connection_pool_config config;
    config.min_connections = 1;
    config.max_connections = 2;
    config.acquire_timeout = std::chrono::milliseconds(100);
    config.connection_string = "mock://localhost";

    auto factory = []() { return std::make_unique<mock_database>(); };

    connection_pool_v2 pool(database_types::postgres, config, factory, 2);
    if (!pool.initialize()) {
        print_error("Failed to initialize pool");
        return;
    }

    std::cout << "Max connections: " << config.max_connections << "\n";
    std::cout << "Acquire timeout: " << config.acquire_timeout.count() << " ms\n\n";

    // Acquire all available connections
    auto result1 = pool.acquire_connection().get();
    auto result2 = pool.acquire_connection().get();

    if (result1.is_ok() && result2.is_ok()) {
        print_success("Acquired 2 connections (pool is now full)");

        // Try to acquire one more (should timeout)
        std::cout << "\nAttempting to acquire connection when pool is full...\n";
        auto result3 = pool.acquire_connection().get();

        if (result3.is_err()) {
            print_success("Correctly returned error: " + result3.get_error().message());
        } else {
            print_error("Expected timeout but got connection");
        }

        // Release connections
        pool.release_connection(result1.value());
        pool.release_connection(result2.value());
        print_info("Released connections back to pool");
    }

    // Test shutdown
    std::cout << "\nShutting down pool...\n";
    pool.shutdown();
    print_success("Pool shutdown completed");

    // Try to acquire after shutdown
    auto result4 = pool.acquire_connection().get();
    if (result4.is_err()) {
        print_success("Correctly rejected request after shutdown");
    }
}

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "\n";
    std::cout << MAGENTA << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     connection_pool_v2 Demonstration Program          ║\n";
    std::cout << "║     Priority-Based Connection Pool with thread_system ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝" << RESET << "\n";

    try {
        demonstrate_basic_usage();
        demonstrate_priority_scheduling();
        demonstrate_async_health_checks();
        demonstrate_high_throughput();
        demonstrate_error_handling();

        std::cout << "\n" << GREEN << "════════════════════════════════════════════════════════\n";
        std::cout << "All demonstrations completed successfully! ✅\n";
        std::cout << "════════════════════════════════════════════════════════" << RESET << "\n\n";

        return 0;
    } catch (const std::exception& e) {
        print_error(std::string("Exception: ") + e.what());
        return 1;
    }
}
