// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file async_queries.cpp
 * @brief Async query execution example
 *
 * This example demonstrates:
 * - Async query submission
 * - Future handling
 * - Multiple concurrent queries
 * - Error handling in async context
 *
 * Usage:
 *   ./async_queries [connection_string]
 */

#include "integrated/unified_database_system.h"
#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <iomanip>

using namespace database::integrated;
using namespace std::chrono;

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

/**
 * Example 1: Single async query
 */
void example_single_async_query(unified_database_system& db) {
    print_header("Example 1: Single Async Query");

    std::cout << "Submitting async query...\n";

    auto start = steady_clock::now();

    // Submit query asynchronously
    auto future = db.execute_async("SELECT 1 as test_value");

    std::cout << "Query submitted, continuing with other work...\n";

    // Do other work while query executes
    std::this_thread::sleep_for(milliseconds(10));
    std::cout << "Other work completed\n";

    // Get result
    std::cout << "Waiting for query result...\n";
    auto result = future.get();

    auto duration = duration_cast<milliseconds>(steady_clock::now() - start);

    if (result.is_ok()) {
        std::cout << "✅ Query succeeded\n";
        std::cout << "   Rows: " << result.value().size() << "\n";
        std::cout << "   Duration: " << duration.count() << " ms\n";
    } else {
        std::cout << "❌ Query failed: " << result.get_error().message() << "\n";
    }
}

/**
 * Example 2: Multiple concurrent queries
 */
void example_concurrent_queries(unified_database_system& db) {
    print_header("Example 2: Multiple Concurrent Queries");

    const int num_queries = 5;
    std::vector<std::future<Result<query_result>>> futures;

    std::cout << "Submitting " << num_queries << " concurrent queries...\n";

    auto start = steady_clock::now();

    // Submit all queries concurrently
    for (int i = 0; i < num_queries; ++i) {
        std::string query = "SELECT " + std::to_string(i) + " as query_id";
        futures.push_back(db.execute_async(query));
    }

    std::cout << "All queries submitted\n";
    std::cout << "Waiting for results...\n";

    // Collect all results
    int succeeded = 0;
    int failed = 0;

    for (size_t i = 0; i < futures.size(); ++i) {
        auto result = futures[i].get();

        if (result.is_ok()) {
            succeeded++;
            std::cout << "  Query " << (i + 1) << ": ✅ Success\n";
        } else {
            failed++;
            std::cout << "  Query " << (i + 1) << ": ❌ Failed - "
                      << result.get_error().message() << "\n";
        }
    }

    auto duration = duration_cast<milliseconds>(steady_clock::now() - start);

    std::cout << "\nResults:\n";
    std::cout << "  Succeeded: " << succeeded << "\n";
    std::cout << "  Failed: " << failed << "\n";
    std::cout << "  Total Duration: " << duration.count() << " ms\n";
    std::cout << "  Avg Duration: " << (duration.count() / num_queries) << " ms/query\n";
}

/**
 * Example 3: Query with timeout
 */
void example_query_with_timeout(unified_database_system& db) {
    print_header("Example 3: Query with Timeout");

    std::cout << "Submitting query with 2 second timeout...\n";

    auto future = db.execute_async("SELECT pg_sleep(1)"); // Sleeps for 1 second

    // Wait for result with timeout
    auto status = future.wait_for(seconds(2));

    if (status == std::future_status::ready) {
        std::cout << "✅ Query completed within timeout\n";
        auto result = future.get();
        if (result.is_ok()) {
            std::cout << "   Result: Success\n";
        } else {
            std::cout << "   Result: " << result.get_error().message() << "\n";
        }
    } else if (status == std::future_status::timeout) {
        std::cout << "⏱️  Query timed out (still executing in background)\n";
    } else {
        std::cout << "⏸️  Query deferred\n";
    }
}

/**
 * Example 4: Batch async operations
 */
void example_batch_operations(unified_database_system& db) {
    print_header("Example 4: Batch Async Operations");

    struct QueryTask {
        std::string name;
        std::string query;
        std::future<Result<query_result>> future;
    };

    std::vector<QueryTask> tasks;

    // Prepare batch of queries
    tasks.push_back({"User count", "SELECT COUNT(*) FROM users", {}});
    tasks.push_back({"Active sessions", "SELECT COUNT(*) FROM sessions WHERE active = true", {}});
    tasks.push_back({"Recent orders", "SELECT COUNT(*) FROM orders WHERE created_at > NOW() - INTERVAL '1 hour'", {}});
    tasks.push_back({"System status", "SELECT 1", {}});

    std::cout << "Submitting batch of " << tasks.size() << " queries...\n";

    auto start = steady_clock::now();

    // Submit all queries
    for (auto& task : tasks) {
        task.future = db.execute_async(task.query);
    }

    std::cout << "All queries submitted, processing results...\n\n";

    // Process results
    for (auto& task : tasks) {
        std::cout << "  " << std::setw(20) << std::left << task.name << ": ";

        auto result = task.future.get();

        if (result.is_ok()) {
            std::cout << "✅ " << result.value().size() << " rows\n";
        } else {
            std::cout << "❌ " << result.get_error().message() << "\n";
        }
    }

    auto duration = duration_cast<milliseconds>(steady_clock::now() - start);
    std::cout << "\nTotal Duration: " << duration.count() << " ms\n";
}

/**
 * Example 5: Error handling in async context
 */
void example_error_handling(unified_database_system& db) {
    print_header("Example 5: Error Handling in Async Context");

    std::cout << "Submitting intentionally invalid query...\n";

    // Submit an invalid query
    auto future = db.execute_async("SELECT * FROM nonexistent_table");

    std::cout << "Query submitted, waiting for result...\n";

    auto result = future.get();

    if (result.is_ok()) {
        std::cout << "✅ Query succeeded (unexpected)\n";
    } else {
        std::cout << "❌ Query failed (expected):\n";
        std::cout << "   Error Code: " << static_cast<int>(result.get_error().code()) << "\n";
        std::cout << "   Error Message: " << result.get_error().message() << "\n";
    }
}

int main(int argc, char* argv[]) {
    print_header("Unified Database System - Async Queries Example");

    // Create database instance
    std::cout << "Creating database instance with async support...\n";

    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .enable_async(8)  // 8 async worker threads
        .set_pool_size(2, 10)
        .build();

    if (!db) {
        std::cerr << "❌ Failed to create database instance\n";
        return 1;
    }

    std::cout << "✅ Database instance created with async support\n";

    // Connect to database (if connection string provided)
    if (argc > 1) {
        std::string conn_string = argv[1];
        std::cout << "Connecting to database...\n";

        auto connect_result = db->connect(conn_string);

        if (connect_result.is_ok()) {
            std::cout << "✅ Connected successfully\n";

            // Run examples
            example_single_async_query(*db);
            example_concurrent_queries(*db);
            example_query_with_timeout(*db);
            example_batch_operations(*db);
            example_error_handling(*db);

            // Show final metrics
            print_header("Final Metrics");
            auto metrics = db->get_metrics();
            std::cout << "Total Queries: " << metrics.total_queries << "\n";
            std::cout << "Successful: " << metrics.successful_queries << "\n";
            std::cout << "Failed: " << metrics.failed_queries << "\n";
            std::cout << "Avg Latency: " << std::fixed << std::setprecision(3)
                      << (metrics.average_latency.count() / 1000.0) << " ms\n";

            // Disconnect
            db->disconnect();
            std::cout << "\n✅ Disconnected\n";

        } else {
            std::cout << "❌ Connection failed: " << connect_result.get_error().message() << "\n";
            std::cout << "\nNote: This example requires a real database connection.\n";
            std::cout << "Usage: " << argv[0] << " \"host=localhost dbname=test user=test password=test\"\n";
            return 1;
        }

    } else {
        std::cout << "\n❌ No connection string provided.\n";
        std::cout << "Usage: " << argv[0] << " [connection_string]\n";
        std::cout << "Example: " << argv[0]
                  << " \"host=localhost dbname=test user=test password=test\"\n";
        return 1;
    }

    print_header("Example Complete");
    std::cout << "✅ All async query examples completed!\n\n";

    return 0;
}
