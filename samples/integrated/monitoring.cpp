// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file monitoring.cpp
 * @brief Monitoring and observability example
 *
 * This example demonstrates:
 * - Metrics collection and access
 * - Health check implementation
 * - Performance monitoring
 * - Real-time statistics
 *
 * Usage:
 *   ./monitoring [connection_string]
 */

#include "integrated/unified_database_system.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace database::integrated;
using namespace std::chrono;

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

void print_separator() {
    std::cout << std::string(70, '-') << "\n";
}

/**
 * Print detailed health check information
 */
void print_health_check(const health_check& health, bool detailed = true) {
    std::cout << "Health Check Report:\n";
    print_separator();

    std::cout << "  Overall Status: ";
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

    std::cout << "  Database Connected: " << (health.is_connected ? "Yes ✓" : "No ✗") << "\n";

    if (detailed) {
        std::cout << "\n  Component Health:\n";
        std::cout << "    Logger:       " << (health.logger_healthy ? "✓ Healthy" : "✗ Unhealthy") << "\n";
        std::cout << "    Monitor:      " << (health.monitor_healthy ? "✓ Healthy" : "✗ Unhealthy") << "\n";
        std::cout << "    Thread Pool:  " << (health.thread_pool_healthy ? "✓ Healthy" : "✗ Unhealthy") << "\n";

        std::cout << "\n  Connection Pool:\n";
        std::cout << "    Utilization:  " << std::fixed << std::setprecision(1)
                  << (health.connection_pool_utilization * 100) << "%\n";
        std::cout << "    Healthy:      " << (health.connection_pool_healthy ? "Yes" : "No") << "\n";

        if (!health.issues.empty()) {
            std::cout << "\n  ⚠️  Issues Detected:\n";
            for (const auto& issue : health.issues) {
                std::cout << "    - " << issue << "\n";
            }
        }
    }

    print_separator();
}

/**
 * Print detailed metrics information
 */
void print_metrics(const database_metrics& metrics, bool detailed = true) {
    std::cout << "Database Metrics:\n";
    print_separator();

    std::cout << "  Query Statistics:\n";
    std::cout << "    Total Queries:      " << std::setw(10) << metrics.total_queries << "\n";
    std::cout << "    Successful:         " << std::setw(10) << metrics.successful_queries << "\n";
    std::cout << "    Failed:             " << std::setw(10) << metrics.failed_queries << "\n";

    if (metrics.total_queries > 0) {
        double success_rate = (static_cast<double>(metrics.successful_queries) / metrics.total_queries) * 100;
        std::cout << "    Success Rate:       " << std::fixed << std::setprecision(2)
                  << std::setw(9) << success_rate << " %\n";
    }

    std::cout << "\n  Performance:\n";
    std::cout << "    Queries/sec:        " << std::fixed << std::setprecision(2)
              << std::setw(10) << metrics.queries_per_second << "\n";
    std::cout << "    Avg Latency:        " << std::fixed << std::setprecision(3)
              << std::setw(10) << (metrics.average_latency.count() / 1000.0) << " ms\n";
    std::cout << "    Min Latency:        " << std::fixed << std::setprecision(3)
              << std::setw(10) << (metrics.min_latency.count() / 1000.0) << " ms\n";
    std::cout << "    Max Latency:        " << std::fixed << std::setprecision(3)
              << std::setw(10) << (metrics.max_latency.count() / 1000.0) << " ms\n";

    if (detailed) {
        std::cout << "\n  Connection Pool:\n";
        std::cout << "    Pool Size:          " << std::setw(10) << metrics.pool_size << "\n";
        std::cout << "    Active:             " << std::setw(10) << metrics.active_connections << "\n";
        std::cout << "    Idle:               " << std::setw(10) << metrics.idle_connections << "\n";
        std::cout << "    Wait Queue:         " << std::setw(10) << metrics.wait_queue_size << "\n";

        std::cout << "\n  Transactions:\n";
        std::cout << "    Started:            " << std::setw(10) << metrics.transactions_started << "\n";
        std::cout << "    Committed:          " << std::setw(10) << metrics.transactions_committed << "\n";
        std::cout << "    Rolled Back:        " << std::setw(10) << metrics.transactions_rolled_back << "\n";
    }

    print_separator();
}

/**
 * Example 1: Real-time monitoring
 */
void example_realtime_monitoring(unified_database_system& db) {
    print_header("Example 1: Real-Time Monitoring");

    std::cout << "Monitoring database for 10 seconds...\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    const int duration_seconds = 10;
    const int interval_ms = 1000;

    for (int i = 0; i < duration_seconds; ++i) {
        auto metrics = db.get_metrics();

        std::cout << "[" << std::setw(2) << (i + 1) << "s] "
                  << "QPS: " << std::fixed << std::setprecision(1) << std::setw(6)
                  << metrics.queries_per_second
                  << " | Latency: " << std::fixed << std::setprecision(3) << std::setw(8)
                  << (metrics.average_latency.count() / 1000.0) << " ms"
                  << " | Active: " << std::setw(2) << metrics.active_connections
                  << " | Idle: " << std::setw(2) << metrics.idle_connections
                  << " | Total: " << std::setw(4) << metrics.total_queries
                  << "\n";

        std::this_thread::sleep_for(milliseconds(interval_ms));
    }

    std::cout << "\n✅ Monitoring complete\n";
}

/**
 * Example 2: Health check monitoring
 */
void example_health_monitoring(unified_database_system& db) {
    print_header("Example 2: Health Check Monitoring");

    std::cout << "Performing periodic health checks...\n\n";

    for (int i = 0; i < 3; ++i) {
        std::cout << "Health Check #" << (i + 1) << ":\n";

        auto health = db.check_health();
        print_health_check(health, false);

        if (i < 2) {
            std::this_thread::sleep_for(seconds(2));
        }
    }

    std::cout << "\n✅ Health checks complete\n";
}

/**
 * Example 3: Performance profiling
 */
void example_performance_profiling(unified_database_system& db) {
    print_header("Example 3: Performance Profiling");

    std::cout << "Running performance test...\n";

    // Capture initial metrics
    auto metrics_before = db.get_metrics();
    auto start = steady_clock::now();

    // Execute test queries
    const int num_queries = 100;
    int succeeded = 0;
    int failed = 0;

    std::cout << "Executing " << num_queries << " test queries...\n";

    for (int i = 0; i < num_queries; ++i) {
        auto result = db.execute("SELECT 1");
        if (result.is_ok()) {
            succeeded++;
        } else {
            failed++;
        }

        // Small delay to simulate realistic workload
        if (i % 10 == 0) {
            std::this_thread::sleep_for(milliseconds(10));
        }
    }

    auto duration = duration_cast<milliseconds>(steady_clock::now() - start);

    // Capture final metrics
    auto metrics_after = db.get_metrics();

    std::cout << "\nPerformance Report:\n";
    print_separator();

    std::cout << "  Execution:\n";
    std::cout << "    Duration:           " << duration.count() << " ms\n";
    std::cout << "    Queries Executed:   " << num_queries << "\n";
    std::cout << "    Succeeded:          " << succeeded << "\n";
    std::cout << "    Failed:             " << failed << "\n";
    std::cout << "    Throughput:         " << std::fixed << std::setprecision(2)
              << (num_queries * 1000.0 / duration.count()) << " queries/sec\n";

    std::cout << "\n  Metrics Delta:\n";
    std::cout << "    Total Queries:      +" << (metrics_after.total_queries - metrics_before.total_queries) << "\n";
    std::cout << "    Successful:         +" << (metrics_after.successful_queries - metrics_before.successful_queries) << "\n";
    std::cout << "    Failed:             +" << (metrics_after.failed_queries - metrics_before.failed_queries) << "\n";

    print_separator();

    std::cout << "\n✅ Performance profiling complete\n";
}

/**
 * Example 4: Alerting based on metrics
 */
void example_alerting(unified_database_system& db) {
    print_header("Example 4: Alerting Based on Metrics");

    std::cout << "Monitoring for alert conditions...\n\n";

    // Define alert thresholds
    const double high_latency_threshold = 100.0;  // ms
    const double high_pool_utilization = 0.8;     // 80%
    const double low_success_rate = 0.95;         // 95%

    auto metrics = db.get_metrics();
    auto health = db.check_health();

    bool alerts_triggered = false;

    // Check for high latency
    double avg_latency_ms = metrics.average_latency.count() / 1000.0;
    if (avg_latency_ms > high_latency_threshold) {
        std::cout << "⚠️  ALERT: High query latency detected!\n";
        std::cout << "   Current: " << std::fixed << std::setprecision(3)
                  << avg_latency_ms << " ms\n";
        std::cout << "   Threshold: " << high_latency_threshold << " ms\n\n";
        alerts_triggered = true;
    }

    // Check for high pool utilization
    if (health.connection_pool_utilization > high_pool_utilization) {
        std::cout << "⚠️  ALERT: High connection pool utilization!\n";
        std::cout << "   Current: " << (health.connection_pool_utilization * 100) << "%\n";
        std::cout << "   Threshold: " << (high_pool_utilization * 100) << "%\n\n";
        alerts_triggered = true;
    }

    // Check for low success rate
    if (metrics.total_queries > 0) {
        double success_rate = static_cast<double>(metrics.successful_queries) / metrics.total_queries;
        if (success_rate < low_success_rate) {
            std::cout << "⚠️  ALERT: Low query success rate!\n";
            std::cout << "   Current: " << (success_rate * 100) << "%\n";
            std::cout << "   Threshold: " << (low_success_rate * 100) << "%\n\n";
            alerts_triggered = true;
        }
    }

    // Check for unhealthy status
    if (health.status != health_status::healthy) {
        std::cout << "⚠️  ALERT: System health degraded!\n";
        std::cout << "   Status: ";
        switch (health.status) {
            case health_status::degraded: std::cout << "Degraded\n"; break;
            case health_status::failed: std::cout << "Failed\n"; break;
            case health_status::critical: std::cout << "Critical\n"; break;
            default: std::cout << "Unknown\n";
        }
        std::cout << "\n";
        alerts_triggered = true;
    }

    if (!alerts_triggered) {
        std::cout << "✅ No alerts triggered - system operating normally\n";
    }

    std::cout << "\n✅ Alert monitoring complete\n";
}

int main(int argc, char* argv[]) {
    print_header("Unified Database System - Monitoring Example");

    // Create database instance with monitoring enabled
    std::cout << "Creating database instance with monitoring...\n";

    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .enable_async(4)
        .set_pool_size(5, 20)
        .set_slow_query_threshold(milliseconds(100))
        .build();

    if (!db) {
        std::cerr << "❌ Failed to create database instance\n";
        return 1;
    }

    std::cout << "✅ Database instance created\n";

    // Initial health check
    print_header("Initial Health Check");
    auto health = db->check_health();
    print_health_check(health);

    // Initial metrics
    print_header("Initial Metrics");
    auto metrics = db->get_metrics();
    print_metrics(metrics);

    // Connect to database (if connection string provided)
    if (argc > 1) {
        std::string conn_string = argv[1];
        std::cout << "\nConnecting to database...\n";

        auto connect_result = db->connect(conn_string);

        if (connect_result.is_ok()) {
            std::cout << "✅ Connected successfully\n";

            // Run monitoring examples
            example_realtime_monitoring(*db);
            example_health_monitoring(*db);
            example_performance_profiling(*db);
            example_alerting(*db);

            // Final report
            print_header("Final Report");
            health = db->check_health();
            print_health_check(health);

            metrics = db->get_metrics();
            print_metrics(metrics);

            // Disconnect
            db->disconnect();
            std::cout << "\n✅ Disconnected\n";

        } else {
            std::cout << "❌ Connection failed: " << connect_result.error().message() << "\n";
            std::cout << "\nNote: This example requires a real database connection.\n";
        }

    } else {
        std::cout << "\n❌ No connection string provided.\n";
        std::cout << "Usage: " << argv[0] << " [connection_string]\n";
        return 1;
    }

    print_header("Example Complete");
    std::cout << "✅ All monitoring examples completed!\n\n";

    return 0;
}
