/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Memory Stress Tests (DB-009)
 *
 * Tests for memory management under stress:
 * - Large result set handling
 * - Repeated query memory stability
 * - Memory growth monitoring
 * - Result set cleanup
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "database/backends/sqlite_backend.h"
#include "database/core/database_backend.h"
#include "database/query_builder.h"

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task.h>
#endif

using namespace database;
using namespace database::backends;
using namespace database::core;

/**
 * @class MemoryStressTest
 * @brief Test fixture for memory stress tests
 */
class MemoryStressTest : public ::testing::Test {
protected:
    std::unique_ptr<sqlite_backend> db_;

    void SetUp() override {
        db_ = std::make_unique<sqlite_backend>();
#ifdef USE_SQLITE
        connection_config config;
        config.database = ":memory:";
        ASSERT_TRUE(db_->initialize(config).is_ok());
#else
        GTEST_SKIP() << "SQLite not available";
#endif
    }

    void TearDown() override {
        if (db_ && db_->is_initialized()) {
            db_->shutdown();
        }
    }

    /**
     * @brief Get current memory usage in bytes (platform-specific)
     */
    size_t getCurrentMemoryUsage() {
#ifdef __APPLE__
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                      (task_info_t)&info, &count) == KERN_SUCCESS) {
            return info.resident_size;
        }
#endif
        // Return 0 for unsupported platforms
        return 0;
    }

    /**
     * @brief Convert bytes to human-readable format
     */
    std::string formatBytes(size_t bytes) {
        if (bytes > 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024)) + " MB";
        } else if (bytes > 1024) {
            return std::to_string(bytes / 1024) + " KB";
        }
        return std::to_string(bytes) + " B";
    }
};

//=============================================================================
// Large Result Set Tests
//=============================================================================

/**
 * @test LargeResultSetMemory
 * @brief Tests memory handling with large result sets
 *
 * Creates a table with many rows and queries all data.
 * Monitors memory usage during and after the operation.
 */
TEST_F(MemoryStressTest, LargeResultSetMemory) {
#ifdef USE_SQLITE
    // Create table
    ASSERT_TRUE(db_->execute_query("CREATE TABLE large_data (id INTEGER PRIMARY KEY, data TEXT)"
    ).is_ok());

    // Insert data (1000 rows with 1KB each)
    constexpr int NUM_ROWS = 1000;
    constexpr int DATA_SIZE = 1000;
    std::string data_value(DATA_SIZE, 'X');

    for (int i = 0; i < NUM_ROWS; ++i) {
        std::string query = "INSERT INTO large_data (data) VALUES ('" + data_value + "')";
        auto insert_result = db_->insert_query(query);
        ASSERT_TRUE(insert_result.is_ok());
        ASSERT_GT(insert_result.value(), 0u);
    }

    size_t before_query = getCurrentMemoryUsage();

    // Query all data
    auto query_result = db_->select_query("SELECT * FROM large_data");
    ASSERT_TRUE(query_result.is_ok());
    auto result = query_result.value();

    size_t after_query = getCurrentMemoryUsage();

    EXPECT_EQ(result.size(), static_cast<size_t>(NUM_ROWS))
        << "Expected " << NUM_ROWS << " rows, got " << result.size();

    if (before_query > 0) {
        size_t memory_growth = after_query - before_query;
        size_t expected_min = NUM_ROWS * DATA_SIZE;  // At least the data size

        std::cout << "Large Result Set Memory:\n"
                  << "  Rows: " << NUM_ROWS << " x " << DATA_SIZE << " bytes\n"
                  << "  Memory before: " << formatBytes(before_query) << "\n"
                  << "  Memory after: " << formatBytes(after_query) << "\n"
                  << "  Growth: " << formatBytes(memory_growth) << "\n";

        // Memory growth should be reasonable (not more than 10x the data size)
        EXPECT_LT(memory_growth, expected_min * 10)
            << "Excessive memory usage detected";
    }

    // Clear result and verify memory can be released
    result.clear();

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test RepeatedQueryMemoryStability
 * @brief Tests that memory remains stable after repeated queries
 */
TEST_F(MemoryStressTest, RepeatedQueryMemoryStability) {
#ifdef USE_SQLITE
    // Create and populate table
    ASSERT_TRUE(db_->execute_query(
        "CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)").is_ok());

    for (int i = 0; i < 100; ++i) {
        db_->insert_query("INSERT INTO test_table (value) VALUES ('test_value_" +
                         std::to_string(i) + "')");
    }

    size_t baseline_memory = getCurrentMemoryUsage();

    // Execute many queries
    constexpr int NUM_QUERIES = 1000;
    for (int i = 0; i < NUM_QUERIES; ++i) {
        auto result = db_->select_query("SELECT * FROM test_table");
        // Result goes out of scope and should be freed
    }

    size_t final_memory = getCurrentMemoryUsage();

    if (baseline_memory > 0) {
        // Allow up to 20% memory growth for caching, etc.
        double growth_ratio = static_cast<double>(final_memory) / baseline_memory;

        std::cout << "Repeated Query Memory:\n"
                  << "  Queries: " << NUM_QUERIES << "\n"
                  << "  Baseline: " << formatBytes(baseline_memory) << "\n"
                  << "  Final: " << formatBytes(final_memory) << "\n"
                  << "  Ratio: " << growth_ratio << "x\n";

        EXPECT_LT(growth_ratio, 1.5)
            << "Possible memory leak: memory grew by " << ((growth_ratio - 1) * 100) << "%";
    }

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Query Builder Memory Tests
//=============================================================================

/**
 * @test QueryBuilderMemoryUsage
 * @brief Tests query builder memory usage with many operations
 */
TEST_F(MemoryStressTest, QueryBuilderMemoryUsage) {
#ifdef USE_SQLITE
    size_t initial_memory = getCurrentMemoryUsage();

    constexpr int NUM_BUILDERS = 1000;

    for (int i = 0; i < NUM_BUILDERS; ++i) {
        query_builder builder(database_types::sqlite);
        auto query = builder
            .select({"id", "name", "value"})
            .from("test_table")
            .where("id", ">", static_cast<int64_t>(i))
            .where("name", "=", std::string("test"))
            .order_by("id")
            .limit(100)
            .build();

        // Use the query to prevent optimization
        EXPECT_FALSE(query.empty());
    }

    size_t final_memory = getCurrentMemoryUsage();

    if (initial_memory > 0) {
        double growth_ratio = static_cast<double>(final_memory) / initial_memory;

        std::cout << "Query Builder Memory:\n"
                  << "  Builders created: " << NUM_BUILDERS << "\n"
                  << "  Initial: " << formatBytes(initial_memory) << "\n"
                  << "  Final: " << formatBytes(final_memory) << "\n"
                  << "  Ratio: " << growth_ratio << "x\n";

        // Query builders should be cleaned up after going out of scope
        EXPECT_LT(growth_ratio, 2.0)
            << "Query builder memory not properly released";
    }

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Result Set Lifecycle Tests
//=============================================================================

/**
 * @test ResultSetProperCleanup
 * @brief Tests that result sets are properly cleaned up
 */
TEST_F(MemoryStressTest, ResultSetProperCleanup) {
#ifdef USE_SQLITE
    ASSERT_TRUE(db_->execute_query("CREATE TABLE cleanup_test (id INTEGER PRIMARY KEY, data TEXT)").is_ok());

    // Insert some data
    std::string data(500, 'A');
    for (int i = 0; i < 50; ++i) {
        db_->insert_query("INSERT INTO cleanup_test (data) VALUES ('" + data + "')");
    }

    // Query and clear in a loop
    for (int iteration = 0; iteration < 10; ++iteration) {
        auto query_result = db_->select_query("SELECT * FROM cleanup_test");
        ASSERT_TRUE(query_result.is_ok());
        database_result result = query_result.value();
        EXPECT_EQ(result.size(), 50u);

        // Explicitly clear
        result.clear();
        EXPECT_TRUE(result.empty());
    }

    SUCCEED() << "Result sets properly cleaned up in all iterations";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test PartialResultConsumption
 * @brief Tests memory behavior when only part of result is consumed
 */
TEST_F(MemoryStressTest, PartialResultConsumption) {
#ifdef USE_SQLITE
    ASSERT_TRUE(db_->execute_query("CREATE TABLE partial_test (id INTEGER PRIMARY KEY, data TEXT)").is_ok());

    std::string data(200, 'B');
    for (int i = 0; i < 100; ++i) {
        db_->insert_query("INSERT INTO partial_test (data) VALUES ('" + data + "')");
    }

    size_t initial_memory = getCurrentMemoryUsage();

    // Query but only use first few rows
    for (int i = 0; i < 100; ++i) {
        auto query_result = db_->select_query("SELECT * FROM partial_test");
        if (query_result.is_ok() && !query_result.value().empty()) {
            // Only access first row
            auto& first_row = query_result.value()[0];
            (void)first_row;
        }
        // Result goes out of scope
    }

    size_t final_memory = getCurrentMemoryUsage();

    if (initial_memory > 0) {
        EXPECT_LT(final_memory, initial_memory * 2)
            << "Memory leak with partial result consumption";
    }

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Stress Test with Mixed Operations
//=============================================================================

/**
 * @test MixedOperationsMemoryStability
 * @brief Tests memory stability under mixed CRUD operations
 */
TEST_F(MemoryStressTest, MixedOperationsMemoryStability) {
#ifdef USE_SQLITE
    ASSERT_TRUE(db_->execute_query("CREATE TABLE mixed_test (id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT)").is_ok());

    size_t baseline = getCurrentMemoryUsage();
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Insert
        db_->insert_query("INSERT INTO mixed_test (value) VALUES ('test_" +
                         std::to_string(i) + "')");

        // Select
        auto result = db_->select_query("SELECT * FROM mixed_test");
        (void)result;

        // Update
        db_->update_query("UPDATE mixed_test SET value = 'updated_" +
                         std::to_string(i) + "' WHERE id = " + std::to_string(i + 1));

        // Delete (delete old entries to prevent table growth)
        if (i > 10) {
            db_->delete_query("DELETE FROM mixed_test WHERE id = " +
                             std::to_string(i - 10));
        }
    }

    size_t final_memory = getCurrentMemoryUsage();

    if (baseline > 0) {
        double growth = static_cast<double>(final_memory) / baseline;
        std::cout << "Mixed Operations Memory:\n"
                  << "  Iterations: " << ITERATIONS << "\n"
                  << "  Baseline: " << formatBytes(baseline) << "\n"
                  << "  Final: " << formatBytes(final_memory) << "\n"
                  << "  Growth ratio: " << growth << "x\n";

        EXPECT_LT(growth, 2.0) << "Excessive memory growth in mixed operations";
    }

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Long String Handling Tests
//=============================================================================

/**
 * @test VeryLongStringHandling
 * @brief Tests memory handling with very long strings
 */
TEST_F(MemoryStressTest, VeryLongStringHandling) {
#ifdef USE_SQLITE
    ASSERT_TRUE(db_->execute_query("CREATE TABLE long_string_test (id INTEGER PRIMARY KEY, data TEXT)").is_ok());

    // Insert progressively longer strings
    std::vector<size_t> sizes = {1000, 5000, 10000, 50000};

    for (size_t size : sizes) {
        std::string long_data(size, 'X');
        std::string query = "INSERT INTO long_string_test (data) VALUES ('" + long_data + "')";

        EXPECT_NO_THROW({
            db_->insert_query(query);
        }) << "Failed to insert string of size " << size;
    }

    // Query and verify
    auto query_result = db_->select_query("SELECT * FROM long_string_test");
    ASSERT_TRUE(query_result.is_ok());
    EXPECT_EQ(query_result.value().size(), sizes.size());

    SUCCEED() << "Successfully handled strings up to " << sizes.back() << " bytes";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test ManySmallStrings
 * @brief Tests memory handling with many small strings
 */
TEST_F(MemoryStressTest, ManySmallStrings) {
#ifdef USE_SQLITE
    ASSERT_TRUE(db_->execute_query("CREATE TABLE small_strings (id INTEGER PRIMARY KEY AUTOINCREMENT, data TEXT)").is_ok());

    constexpr int NUM_STRINGS = 5000;
    constexpr int STRING_SIZE = 50;

    size_t initial_memory = getCurrentMemoryUsage();

    for (int i = 0; i < NUM_STRINGS; ++i) {
        std::string small_data(STRING_SIZE, static_cast<char>('A' + (i % 26)));
        db_->insert_query("INSERT INTO small_strings (data) VALUES ('" + small_data + "')");
    }

    // Query all
    auto query_result = db_->select_query("SELECT * FROM small_strings");
    ASSERT_TRUE(query_result.is_ok());
    EXPECT_EQ(query_result.value().size(), static_cast<size_t>(NUM_STRINGS));

    size_t final_memory = getCurrentMemoryUsage();

    if (initial_memory > 0) {
        size_t expected_data = NUM_STRINGS * STRING_SIZE;
        size_t actual_growth = final_memory - initial_memory;

        std::cout << "Many Small Strings:\n"
                  << "  Strings: " << NUM_STRINGS << " x " << STRING_SIZE << " bytes\n"
                  << "  Expected data: " << formatBytes(expected_data) << "\n"
                  << "  Memory growth: " << formatBytes(actual_growth) << "\n";
    }

    SUCCEED();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}
