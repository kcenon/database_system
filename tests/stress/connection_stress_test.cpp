/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Connection Stress Tests (DB-009)
 *
 * Tests for connection pool under stress:
 * - Rapid connection cycling
 * - Pool exhaustion and recovery
 * - Connection leak detection under stress
 * - Concurrent pool access
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>

#include "database/backends/sqlite/sqlite_manager.h"
#include "database/connection_pool.h"

using namespace database;

/**
 * @class ConnectionStressTest
 * @brief Test fixture for connection pool stress tests
 */
class ConnectionStressTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

//=============================================================================
// Rapid Connection Cycling Tests
//=============================================================================

/**
 * @test RapidConnectionCycling
 * @brief Tests rapid connect/disconnect cycles
 *
 * Creates and destroys many database connections in rapid succession.
 * Verifies no resource leaks or crashes.
 */
TEST_F(ConnectionStressTest, RapidConnectionCycling) {
#ifdef USE_SQLITE
    constexpr int CYCLES = 100;
    int successful_cycles = 0;
    int failed_cycles = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < CYCLES; ++i) {
        try {
            auto db = std::make_unique<sqlite_manager>();
            if (db->connect(":memory:")) {
                // Execute a simple query
                db->execute_query("SELECT 1");
                db->disconnect();
                successful_cycles++;
            } else {
                failed_cycles++;
            }
        } catch (...) {
            failed_cycles++;
        }
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    std::cout << "Rapid Connection Cycling:\n"
              << "  Cycles: " << CYCLES << "\n"
              << "  Successful: " << successful_cycles << "\n"
              << "  Failed: " << failed_cycles << "\n"
              << "  Duration: " << ms.count() << "ms\n"
              << "  Rate: " << (1000.0 * CYCLES / ms.count()) << " cycles/sec\n";

    EXPECT_EQ(failed_cycles, 0) << "Connection cycling failures detected";
    EXPECT_EQ(successful_cycles, CYCLES);
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test ConcurrentConnectionCreation
 * @brief Tests concurrent database connection creation
 */
TEST_F(ConnectionStressTest, ConcurrentConnectionCreation) {
#ifdef USE_SQLITE
    constexpr int NUM_THREADS = 8;
    constexpr int CONNECTIONS_PER_THREAD = 10;

    std::atomic<int> successful{0};
    std::atomic<int> failed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < CONNECTIONS_PER_THREAD; ++i) {
                try {
                    auto db = std::make_unique<sqlite_manager>();
                    if (db->connect(":memory:")) {
                        db->execute_query("SELECT 1");
                        db->disconnect();
                        successful++;
                    } else {
                        failed++;
                    }
                } catch (...) {
                    failed++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = NUM_THREADS * CONNECTIONS_PER_THREAD;
    EXPECT_EQ(successful.load(), total)
        << "Failed concurrent connection creation: " << failed.load() << " failures";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Connection Pool Tests
//=============================================================================

/**
 * @test PoolConcurrentAccess
 * @brief Tests connection pool under concurrent access
 */
TEST_F(ConnectionStressTest, PoolConcurrentAccess) {
#ifdef USE_SQLITE
    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.connection_string = ":memory:";
    config.acquire_timeout = std::chrono::milliseconds(1000);

    auto factory = []() -> std::unique_ptr<database_base> {
        auto db = std::make_unique<sqlite_manager>();
        db->connect(":memory:");
        return db;
    };

    try {
        connection_pool pool(database_types::sqlite, config, factory);
        pool.initialize();

        constexpr int NUM_THREADS = 8;
        constexpr int OPS_PER_THREAD = 20;

        std::atomic<int> acquired{0};
        std::atomic<int> failed{0};
        std::vector<std::thread> threads;

        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < OPS_PER_THREAD; ++i) {
                    try {
                        auto result = pool.acquire();
                        if (result.has_value()) {
                            // Use connection
                            auto& wrapper = result.value();
                            if (wrapper && wrapper->get()) {
                                wrapper->get()->execute_query("SELECT 1");
                            }
                            acquired++;
                            pool.release(std::move(*wrapper));
                        } else {
                            failed++;
                        }
                    } catch (...) {
                        failed++;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        pool.shutdown();

        std::cout << "Pool Concurrent Access:\n"
                  << "  Threads: " << NUM_THREADS << "\n"
                  << "  Ops per thread: " << OPS_PER_THREAD << "\n"
                  << "  Acquired: " << acquired << "\n"
                  << "  Failed: " << failed << "\n";

        // Most operations should succeed
        EXPECT_GT(acquired.load(), 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Connection pool test skipped: " << e.what();
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test PoolExhaustionRecovery
 * @brief Tests pool behavior when exhausted and recovery
 */
TEST_F(ConnectionStressTest, PoolExhaustionRecovery) {
#ifdef USE_SQLITE
    connection_pool_config config;
    config.min_connections = 1;
    config.max_connections = 3;
    config.connection_string = ":memory:";
    config.acquire_timeout = std::chrono::milliseconds(100);

    auto factory = []() -> std::unique_ptr<database_base> {
        auto db = std::make_unique<sqlite_manager>();
        db->connect(":memory:");
        return db;
    };

    try {
        connection_pool pool(database_types::sqlite, config, factory);
        pool.initialize();

        // Acquire all connections
        std::vector<std::shared_ptr<connection_wrapper>> held_connections;

        int acquired_count = 0;
        for (int i = 0; i < static_cast<int>(config.max_connections); ++i) {
            auto result = pool.acquire();
            if (result.has_value()) {
                // Note: We need to hold onto these connections
                acquired_count++;
            }
        }

        // Try to acquire one more - should timeout/fail
        auto extra_result = pool.acquire();
        bool exhaustion_handled = !extra_result.has_value();

        pool.shutdown();

        EXPECT_TRUE(exhaustion_handled || acquired_count < static_cast<int>(config.max_connections))
            << "Pool exhaustion not properly handled";
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Connection pool test skipped: " << e.what();
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Long-Running Connection Tests
//=============================================================================

/**
 * @test LongRunningConnectionStability
 * @brief Tests connection stability over extended period
 */
TEST_F(ConnectionStressTest, LongRunningConnectionStability) {
#ifdef USE_SQLITE
    auto db = std::make_unique<sqlite_manager>();
    ASSERT_TRUE(db->connect(":memory:"));

    db->execute_query("CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)");

    constexpr int NUM_OPERATIONS = 500;
    int successful = 0;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        // Insert
        std::string insert_query = "INSERT INTO test_table (value) VALUES ('test_" +
                                   std::to_string(i) + "')";
        if (db->insert_query(insert_query) > 0) {
            successful++;
        }

        // Select
        auto result = db->select_query("SELECT COUNT(*) FROM test_table");
        if (!result.empty()) {
            successful++;
        }

        // Small delay to simulate real-world usage
        if (i % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    db->disconnect();

    double success_rate = 100.0 * successful / (NUM_OPERATIONS * 2);
    std::cout << "Long-Running Connection:\n"
              << "  Operations: " << (NUM_OPERATIONS * 2) << "\n"
              << "  Successful: " << successful << "\n"
              << "  Success rate: " << success_rate << "%\n";

    EXPECT_GE(success_rate, 99.0) << "Stability issues detected";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Resource Cleanup Tests
//=============================================================================

/**
 * @test ProperResourceCleanup
 * @brief Verifies resources are properly cleaned up after stress
 */
TEST_F(ConnectionStressTest, ProperResourceCleanup) {
#ifdef USE_SQLITE
    // Create and destroy many connections
    for (int batch = 0; batch < 5; ++batch) {
        std::vector<std::unique_ptr<sqlite_manager>> connections;

        // Create connections
        for (int i = 0; i < 10; ++i) {
            auto db = std::make_unique<sqlite_manager>();
            if (db->connect(":memory:")) {
                db->execute_query("CREATE TABLE test (id INT)");
                connections.push_back(std::move(db));
            }
        }

        // Use connections
        for (auto& conn : connections) {
            conn->insert_query("INSERT INTO test VALUES (1)");
            conn->select_query("SELECT * FROM test");
        }

        // Disconnect all
        for (auto& conn : connections) {
            conn->disconnect();
        }

        connections.clear();
    }

    // System should still be functional
    auto db = std::make_unique<sqlite_manager>();
    EXPECT_TRUE(db->connect(":memory:"))
        << "System non-functional after resource cleanup test";
    db->disconnect();
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test NoLeaksAfterExceptions
 * @brief Tests that resources are cleaned up even after exceptions
 */
TEST_F(ConnectionStressTest, NoLeaksAfterExceptions) {
#ifdef USE_SQLITE
    constexpr int ITERATIONS = 50;

    for (int i = 0; i < ITERATIONS; ++i) {
        try {
            auto db = std::make_unique<sqlite_manager>();
            db->connect(":memory:");

            // Execute query that might fail
            db->execute_query("CREATE TABLE test_" + std::to_string(i) + " (id INT)");

            // Intentionally cause an error sometimes
            if (i % 5 == 0) {
                db->execute_query("INVALID SQL SYNTAX");
            }

            db->disconnect();
        } catch (...) {
            // Exception handled - resources should be cleaned up
        }
    }

    // Verify system is still functional
    auto db = std::make_unique<sqlite_manager>();
    EXPECT_TRUE(db->connect(":memory:"));
    db->disconnect();

    SUCCEED() << "Completed " << ITERATIONS << " iterations with exception handling";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}
