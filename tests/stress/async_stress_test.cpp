/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Async Operation Stress Tests (DB-009)
 *
 * Tests for high concurrency and async operations:
 * - High concurrency INSERT operations
 * - Mixed read/write workloads
 * - Query timeout under load
 * - Thread contention handling
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>

#include "database/backends/sqlite_backend.h"
#include "database/core/database_backend.h"
#include "database/query_builder.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

/**
 * @class AsyncStressTest
 * @brief Test fixture for async operation stress tests
 */
class AsyncStressTest : public ::testing::Test {
protected:
    std::unique_ptr<sqlite_backend> db_;

    void SetUp() override {
        db_ = std::make_unique<sqlite_backend>();
#ifdef USE_SQLITE
        connection_config config;
        config.database = ":memory:";
        ASSERT_TRUE(db_->initialize(config).is_ok());
        ASSERT_TRUE(db_->execute_query("CREATE TABLE stress_test ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  thread_id INTEGER,"
            "  value TEXT,"
            "  created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ")").is_ok());
#else
        GTEST_SKIP() << "SQLite not available";
#endif
    }

    void TearDown() override {
        if (db_ && db_->is_initialized()) {
            db_->shutdown();
        }
    }
};

//=============================================================================
// High Concurrency Tests
//=============================================================================

/**
 * @test HighConcurrencyInserts
 * @brief Tests database behavior under high concurrent insert load
 *
 * Spawns multiple threads performing concurrent inserts.
 * Measures success rate and performance.
 */
TEST_F(AsyncStressTest, HighConcurrencyInserts) {
#ifdef USE_SQLITE
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 50;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::future<void>> futures;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string query = "INSERT INTO stress_test (thread_id, value) VALUES (" +
                                    std::to_string(t) + ", 'value_" +
                                    std::to_string(t * OPS_PER_THREAD + i) + "')";
                try {
                    auto result = db_->insert_query(query);
                    if (result.is_ok() && result.value() > 0) {
                        success_count++;
                    } else {
                        failure_count++;
                    }
                } catch (...) {
                    failure_count++;
                }
            }
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    // Report results
    int total = success_count + failure_count;
    double success_rate = (total > 0) ? (100.0 * success_count / total) : 0;

    std::cout << "High Concurrency Insert Test:\n"
              << "  Threads: " << NUM_THREADS << "\n"
              << "  Ops per thread: " << OPS_PER_THREAD << "\n"
              << "  Successful: " << success_count << "\n"
              << "  Failed: " << failure_count << "\n"
              << "  Success rate: " << success_rate << "%\n"
              << "  Duration: " << ms.count() << "ms\n";

    // At least some operations should succeed
    EXPECT_GT(success_count.load(), 0)
        << "No successful operations under concurrent load";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test MixedReadWriteWorkload
 * @brief Tests database under mixed read/write workload
 *
 * Multiple reader and writer threads operating simultaneously.
 */
TEST_F(AsyncStressTest, MixedReadWriteWorkload) {
#ifdef USE_SQLITE
    constexpr int NUM_WRITERS = 3;
    constexpr int NUM_READERS = 5;
    constexpr int DURATION_MS = 2000;

    std::atomic<bool> running{true};
    std::atomic<int> write_ops{0};
    std::atomic<int> read_ops{0};
    std::atomic<int> write_errors{0};
    std::atomic<int> read_errors{0};

    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([&, i]() {
            int counter = 0;
            while (running) {
                try {
                    std::string query = "INSERT INTO stress_test (thread_id, value) VALUES (" +
                                        std::to_string(i) + ", 'write_" +
                                        std::to_string(counter++) + "')";
                    auto result = db_->insert_query(query);
                    if (result.is_ok() && result.value() > 0) {
                        write_ops++;
                    } else {
                        write_errors++;
                    }
                } catch (...) {
                    write_errors++;
                }
            }
        });
    }

    // Readers
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([&]() {
            while (running) {
                try {
                    auto result = db_->select_query("SELECT COUNT(*) FROM stress_test");
                    if (result.is_ok() && !result.value().empty()) {
                        read_ops++;
                    } else {
                        read_errors++;
                    }
                } catch (...) {
                    read_errors++;
                }
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Mixed Read/Write Workload:\n"
              << "  Writers: " << NUM_WRITERS << ", Readers: " << NUM_READERS << "\n"
              << "  Duration: " << DURATION_MS << "ms\n"
              << "  Write ops: " << write_ops << " (errors: " << write_errors << ")\n"
              << "  Read ops: " << read_ops << " (errors: " << read_errors << ")\n";

    // Both reads and writes should have occurred
    EXPECT_GT(write_ops.load(), 0) << "No write operations completed";
    EXPECT_GT(read_ops.load(), 0) << "No read operations completed";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * @test ConcurrentQueryBuilderUsage
 * @brief Tests that query builders work correctly under concurrent use
 */
TEST_F(AsyncStressTest, ConcurrentQueryBuilderUsage) {
#ifdef USE_SQLITE
    constexpr int NUM_THREADS = 8;
    constexpr int QUERIES_PER_THREAD = 100;

    std::atomic<int> build_success{0};
    std::atomic<int> build_failure{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < QUERIES_PER_THREAD; ++i) {
                try {
                    // Each thread uses its own builder (not shared)
                    sql_query_builder builder;
                    auto query = builder
                        .select(std::vector<std::string>{"*"})
                        .from("stress_test")
                        .where("thread_id", "=", static_cast<int64_t>(t))
                        .limit(10)
                        .build();

                    if (!query.empty()) {
                        build_success++;
                    } else {
                        build_failure++;
                    }
                } catch (...) {
                    build_failure++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = build_success + build_failure;
    EXPECT_EQ(total, NUM_THREADS * QUERIES_PER_THREAD);
    EXPECT_EQ(build_failure.load(), 0)
        << "Query builder failures under concurrent use";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test RapidQueryExecution
 * @brief Tests rapid sequential query execution
 */
TEST_F(AsyncStressTest, RapidQueryExecution) {
#ifdef USE_SQLITE
    constexpr int NUM_QUERIES = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    int success = 0;
    int failure = 0;

    for (int i = 0; i < NUM_QUERIES; ++i) {
        try {
            std::string query = "INSERT INTO stress_test (thread_id, value) VALUES (0, 'rapid_" +
                                std::to_string(i) + "')";
            auto result = db_->insert_query(query);
            if (result.is_ok() && result.value() > 0) {
                success++;
            } else {
                failure++;
            }
        } catch (...) {
            failure++;
        }
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    double qps = (ms.count() > 0) ? (1000.0 * success / ms.count()) : 0;

    std::cout << "Rapid Query Execution:\n"
              << "  Queries: " << NUM_QUERIES << "\n"
              << "  Success: " << success << ", Failure: " << failure << "\n"
              << "  Duration: " << ms.count() << "ms\n"
              << "  Throughput: " << qps << " queries/sec\n";

    EXPECT_GT(success, 0);
    EXPECT_EQ(failure, 0) << "Failures during rapid sequential execution";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Stress Recovery Tests
//=============================================================================

/**
 * @test SystemRemainResponsiveAfterLoad
 * @brief Tests that database remains responsive after heavy load
 */
TEST_F(AsyncStressTest, SystemRemainResponsiveAfterLoad) {
#ifdef USE_SQLITE
    // Generate load
    for (int i = 0; i < 100; ++i) {
        db_->insert_query("INSERT INTO stress_test (thread_id, value) VALUES (" +
                         std::to_string(i % 10) + ", 'load_test')");
    }

    // Now test responsiveness
    auto start = std::chrono::high_resolution_clock::now();
    auto result = db_->select_query("SELECT COUNT(*) FROM stress_test");
    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration);

    EXPECT_TRUE(result.is_ok() && !result.value().empty()) << "System unresponsive after load";
    EXPECT_LT(us.count(), 100000) << "Query took too long after load: " << us.count() << "us";

    std::cout << "Post-load query response time: " << us.count() << "us\n";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test NoDataCorruptionUnderConcurrency
 * @brief Verifies data integrity after concurrent operations
 */
TEST_F(AsyncStressTest, NoDataCorruptionUnderConcurrency) {
#ifdef USE_SQLITE
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 25;
    const int EXPECTED_ROWS = NUM_THREADS * OPS_PER_THREAD;

    std::vector<std::thread> threads;
    std::atomic<int> actual_inserts{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string unique_value = "thread_" + std::to_string(t) +
                                          "_op_" + std::to_string(i);
                std::string query = "INSERT INTO stress_test (thread_id, value) VALUES (" +
                                    std::to_string(t) + ", '" + unique_value + "')";
                auto insert_result = db_->insert_query(query);
                if (insert_result.is_ok() && insert_result.value() > 0) {
                    actual_inserts++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify row count
    auto result = db_->select_query("SELECT COUNT(*) as cnt FROM stress_test");
    ASSERT_TRUE(result.is_ok() && !result.value().empty());

    int row_count = 0;
    if (!result.value().empty() && result.value()[0].count("cnt") > 0) {
        auto& cnt_value = result.value()[0].at("cnt");
        if (std::holds_alternative<int64_t>(cnt_value)) {
            row_count = static_cast<int>(std::get<int64_t>(cnt_value));
        } else if (std::holds_alternative<std::string>(cnt_value)) {
            row_count = std::stoi(std::get<std::string>(cnt_value));
        }
    }

    // All successful inserts should be present
    EXPECT_EQ(row_count, actual_inserts.load())
        << "Data corruption: row count mismatch";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Exception Handling Under Stress
//=============================================================================

/**
 * @test GracefulHandlingOfErrors
 * @brief Tests that errors are handled gracefully under concurrent load
 */
TEST_F(AsyncStressTest, GracefulHandlingOfErrors) {
#ifdef USE_SQLITE
    std::atomic<bool> running{true};
    std::atomic<int> errors_caught{0};
    std::vector<std::thread> threads;

    // Spawn threads that will generate some errors
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            while (running) {
                try {
                    // Some valid queries
                    db_->select_query("SELECT * FROM stress_test LIMIT 1");

                    // Some invalid queries (should return error)
                    auto result = db_->select_query("SELECT * FROM nonexistent_table");
                    if (!result.is_ok()) {
                        errors_caught++;
                    }
                } catch (...) {
                    errors_caught++;
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    // System should still be functional
    auto result = db_->select_query("SELECT 1 as test");
    EXPECT_TRUE(result.is_ok() && !result.value().empty()) << "System non-functional after error handling test";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}
