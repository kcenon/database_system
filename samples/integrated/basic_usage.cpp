// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file basic_usage.cpp
 * @brief Basic usage example of unified_database_system
 *
 * This example demonstrates:
 * - Zero-config database initialization
 * - Simple query execution
 * - Error handling with Result<T>
 * - Health check and metrics access
 *
 * Usage:
 *   ./basic_usage [connection_string]
 *
 * Example:
 *   ./basic_usage "host=localhost dbname=testdb user=testuser password=testpass"
 */

#include "integrated/unified_database_system.h"
#include <iostream>
#include <iomanip>

using namespace database::integrated;

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

void print_health_check(const health_check& health) {
    std::cout << "Health Status:\n";
    std::cout << "  Overall: ";

    switch (health.status) {
        case health_status::healthy:
            std::cout << "✅ Healthy\n";
            break;
        case health_status::degraded:
            std::cout << "⚠️  Degraded\n";
            break;
        case health_status::failed:
            std::cout << "❌ Failed\n";
            break;
        case health_status::critical:
            std::cout << "🔥 Critical\n";
            break;
        default:
            std::cout << "❓ Unknown\n";
    }

    std::cout << "  Connected: " << (health.is_connected ? "Yes" : "No") << "\n";
    std::cout << "  Logger: " << (health.logger_healthy ? "✓" : "✗") << "\n";
    std::cout << "  Monitor: " << (health.monitor_healthy ? "✓" : "✗") << "\n";
    std::cout << "  Thread Pool: " << (health.thread_pool_healthy ? "✓" : "✗") << "\n";
    std::cout << "  Pool Utilization: "
              << std::fixed << std::setprecision(1)
              << (health.connection_pool_utilization * 100) << "%\n";

    if (!health.issues.empty()) {
        std::cout << "\n  Issues:\n";
        for (const auto& issue : health.issues) {
            std::cout << "    - " << issue << "\n";
        }
    }
}

void print_metrics(const database_metrics& metrics) {
    std::cout << "Database Metrics:\n";
    std::cout << "  Total Queries: " << metrics.total_queries << "\n";
    std::cout << "  Successful: " << metrics.successful_queries << "\n";
    std::cout << "  Failed: " << metrics.failed_queries << "\n";
    std::cout << "  Queries/sec: " << std::fixed << std::setprecision(2)
              << metrics.queries_per_second << "\n";
    std::cout << "  Avg Latency: " << std::fixed << std::setprecision(3)
              << (metrics.average_latency.count() / 1000.0) << " ms\n";
    std::cout << "  Pool Size: " << metrics.pool_size << "\n";
    std::cout << "  Active Connections: " << metrics.active_connections << "\n";
    std::cout << "  Idle Connections: " << metrics.idle_connections << "\n";
}

int main(int argc, char* argv[]) {
    print_header("Unified Database System - Basic Usage Example");

    // Step 1: Create database instance with zero-config
    std::cout << "Step 1: Creating database instance...\n";

    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .set_pool_size(2, 10)
        .build();

    if (!db) {
        std::cerr << "❌ Failed to create database instance\n";
        return 1;
    }

    std::cout << "✅ Database instance created\n";

    // Step 2: Check initial health status
    print_header("Step 2: Initial Health Check");
    auto health = db->check_health();
    print_health_check(health);

    // Step 3: Connect to database (if connection string provided)
    if (argc > 1) {
        print_header("Step 3: Connecting to Database");

        std::string conn_string = argv[1];
        std::cout << "Connection string: " << conn_string << "\n";

        auto connect_result = db->connect(conn_string);

        if (connect_result.is_ok()) {
            std::cout << "✅ Connected successfully\n";

            // Step 4: Execute a simple query
            print_header("Step 4: Executing Simple Query");

            auto query_result = db->execute("SELECT 1 as test_column");

            if (query_result.is_ok()) {
                auto result = query_result.value();
                std::cout << "✅ Query executed successfully\n";
                std::cout << "   Rows returned: " << result.size() << "\n";
                std::cout << "   Affected rows: " << result.affected_rows << "\n";

                if (!result.empty()) {
                    std::cout << "\n   Result:\n";
                    for (size_t i = 0; i < result.size(); ++i) {
                        const auto& row = result[i];
                        std::cout << "   Row " << (i + 1) << ": ";
                        for (const auto& [col, val] : row) {
                            std::cout << col << "=" << val << " ";
                        }
                        std::cout << "\n";
                    }
                }

            } else {
                std::cout << "❌ Query failed: " << query_result.error().message << "\n";
            }

            // Step 5: Check metrics
            print_header("Step 5: Database Metrics");
            auto metrics = db->get_metrics();
            print_metrics(metrics);

            // Step 6: Final health check
            print_header("Step 6: Final Health Check");
            health = db->check_health();
            print_health_check(health);

            // Step 7: Disconnect
            print_header("Step 7: Disconnecting");
            auto disconnect_result = db->disconnect();
            if (disconnect_result.is_ok()) {
                std::cout << "✅ Disconnected successfully\n";
            }

        } else {
            std::cout << "❌ Connection failed: " << connect_result.error().message << "\n";
            std::cout << "\nNote: This is expected if PostgreSQL is not running.\n";
            std::cout << "The example demonstrates the API even without a real database.\n";
        }

    } else {
        std::cout << "\nℹ️  No connection string provided.\n";
        std::cout << "   Usage: " << argv[0] << " [connection_string]\n";
        std::cout << "   Example: " << argv[0]
                  << " \"host=localhost dbname=test user=test password=test\"\n";
        std::cout << "\n   The program will continue to demonstrate the API.\n";
    }

    // Step 8: Show final metrics (even without connection)
    print_header("Final Metrics");
    auto metrics = db->get_metrics();
    print_metrics(metrics);

    print_header("Example Complete");
    std::cout << "✅ All steps completed successfully!\n\n";

    return 0;
}
