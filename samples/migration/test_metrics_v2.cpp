/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_metrics_v2.cpp
 * @brief Simple test for connection_pool_v2 metrics collection
 */

#include "database/pooling/connection_pool_v2.h"
#include "database/database_base.h"
#include "database/database_types.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace database;
using namespace database::pooling;

/**
 * @class mock_database
 * @brief Mock database implementation for testing
 */
class mock_database : public database_base {
public:
    mock_database() = default;
    ~mock_database() override = default;

    database_types database_type() override { return database_types::postgres; }

    bool connect(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }

    bool disconnect() override { return true; }

    bool create_query(const std::string&) override { return true; }
    unsigned int insert_query(const std::string&) override { return 1; }
    unsigned int update_query(const std::string&) override { return 1; }
    unsigned int delete_query(const std::string&) override { return 1; }
    database_result select_query(const std::string&) override { return database_result{}; }

    bool execute_query(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return true;
    }
};

int main() {
    try {
        std::cout << "=== connection_pool_v2 Metrics Test ===\n\n";

        // Create pool configuration
        connection_pool_config config;
        config.min_connections = 2;
        config.max_connections = 5;
        config.acquire_timeout = std::chrono::seconds(10);
        config.connection_string = "mock://localhost";

        // Create factory
        auto factory = []() -> std::unique_ptr<database_base> {
            return std::make_unique<mock_database>();
        };

        // Create pool
        std::cout << "Creating connection_pool_v2...\n";
        connection_pool_v2 pool(database_types::postgres, config, factory, 4);

        // Initialize
        std::cout << "Initializing pool...\n";
        if (!pool.initialize()) {
            std::cerr << "Failed to initialize pool!\n";
            return 1;
        }

        std::cout << "Pool initialized successfully!\n";
        std::cout << "Using thread_system: " << (pool.is_using_thread_system() ? "YES" : "NO") << "\n\n";

        // Acquire and release connections to generate metrics
        std::cout << "Acquiring 3 connections...\n";
        std::vector<std::future<kcenon::common::Result<std::shared_ptr<connection_wrapper>>>> futures;

        for (int i = 0; i < 3; ++i) {
            auto priority = (i == 0) ? connection_priority::CRITICAL :
                           (i == 1) ? connection_priority::NORMAL_QUERY :
                                     connection_priority::HEALTH_CHECK;

            futures.push_back(pool.acquire_connection(priority));
        }

        // Wait for connections
        std::vector<std::shared_ptr<connection_wrapper>> connections;
        for (auto& future : futures) {
            auto result = future.get();
            if (result.is_ok()) {
                connections.push_back(result.value());
                std::cout << "  ✓ Connection acquired\n";
            } else {
                std::cout << "  ✗ Failed to acquire connection: "
                         << result.error().message << "\n";
            }
        }

        // Small delay to ensure metrics are updated
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Get metrics
        std::cout << "\n=== Metrics Report ===\n";
        auto metrics = pool.get_metrics();

        if (metrics) {
            std::cout << "Total acquisitions: " << metrics->total_acquisitions.load() << "\n";
            std::cout << "Successful acquisitions: " << metrics->successful_acquisitions.load() << "\n";
            std::cout << "Failed acquisitions: " << metrics->failed_acquisitions.load() << "\n";
            std::cout << "Success rate: " << metrics->success_rate() << "%\n";
            std::cout << "Average wait time: " << metrics->average_wait_time_us() << " μs\n";
            std::cout << "Min wait time: " << metrics->min_wait_time_us.load() << " μs\n";
            std::cout << "Max wait time: " << metrics->max_wait_time_us.load() << " μs\n";
            std::cout << "Current active: " << metrics->current_active.load() << "\n";
            std::cout << "Peak active: " << metrics->peak_active.load() << "\n";

#ifdef USE_THREAD_SYSTEM
            std::cout << "\n=== Priority-Specific Metrics ===\n";
            std::cout << "CRITICAL avg: "
                     << metrics->average_wait_time_for_priority(connection_priority::CRITICAL)
                     << " μs\n";
            std::cout << "NORMAL_QUERY avg: "
                     << metrics->average_wait_time_for_priority(connection_priority::NORMAL_QUERY)
                     << " μs\n";
            std::cout << "HEALTH_CHECK avg: "
                     << metrics->average_wait_time_for_priority(connection_priority::HEALTH_CHECK)
                     << " μs\n";
#endif
        } else {
            std::cout << "  Metrics not available\n";
        }

        // Release connections
        std::cout << "\nReleasing connections...\n";
        for (auto& conn : connections) {
            pool.release_connection(conn);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Final metrics
        std::cout << "\n=== Final Metrics ===\n";
        std::cout << "Current active: " << metrics->current_active.load() << "\n";

        // Shutdown
        std::cout << "\nShutting down pool...\n";
        pool.shutdown();

        std::cout << "\n✅ Test completed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << "\n";
        return 1;
    }
}
