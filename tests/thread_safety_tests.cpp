/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "database/connection_pool.h"
#include "database/async/async_operations.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <barrier>

using namespace kcenon::database;
using namespace std::chrono_literals;

class DatabaseThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Concurrent connection acquisition
TEST_F(DatabaseThreadSafetyTest, ConcurrentConnectionAcquisition) {
    connection_pool pool("test_db", 10);

    const int num_threads = 20;
    const int acquisitions_per_thread = 100;

    std::atomic<int> successful_acquisitions{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < acquisitions_per_thread; ++j) {
                try {
                    auto conn = pool.acquire_connection();
                    if (conn) {
                        ++successful_acquisitions;
                        std::this_thread::sleep_for(1ms);
                        pool.release_connection(std::move(conn));
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
    EXPECT_GT(successful_acquisitions.load(), 0);
}

// Test 2: Pool exhaustion handling
TEST_F(DatabaseThreadSafetyTest, PoolExhaustion) {
    connection_pool pool("test_db", 5);

    const int num_threads = 10;
    std::atomic<int> timeout_count{0};
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    std::barrier sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            sync_point.arrive_and_wait();

            auto conn = pool.acquire_connection(100ms);
            if (conn) {
                ++success_count;
                std::this_thread::sleep_for(200ms);
                pool.release_connection(std::move(conn));
            } else {
                ++timeout_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(timeout_count.load(), 0) << "Some threads should timeout";
    EXPECT_GT(success_count.load(), 0) << "Some threads should succeed";
}

// Test 3: Async query races
TEST_F(DatabaseThreadSafetyTest, AsyncQueryRaces) {
    auto db = database_manager::create("test_db");

    const int num_queries = 500;
    std::atomic<int> queries_completed{0};
    std::atomic<int> errors{0};

    std::vector<std::future<query_result>> futures;

    for (int i = 0; i < num_queries; ++i) {
        auto future = db->execute_async("SELECT " + std::to_string(i));
        futures.push_back(std::move(future));
    }

    // Collect results concurrently
    std::vector<std::thread> collectors;
    std::atomic<int> next_index{0};

    for (int i = 0; i < 10; ++i) {
        collectors.emplace_back([&]() {
            while (true) {
                int idx = next_index.fetch_add(1);
                if (idx >= num_queries) break;

                try {
                    auto result = futures[idx].get();
                    ++queries_completed;
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : collectors) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(queries_completed.load(), num_queries);
}

// Test 4: Transaction concurrency
TEST_F(DatabaseThreadSafetyTest, TransactionConcurrency) {
    auto db = database_manager::create("test_db");

    const int num_transactions = 100;
    std::atomic<int> committed{0};
    std::atomic<int> rolled_back{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_transactions; ++i) {
        threads.emplace_back([&, trans_id = i]() {
            try {
                auto trans = db->begin_transaction();

                trans->execute("INSERT INTO test VALUES (" + std::to_string(trans_id) + ")");

                if (trans_id % 5 == 0) {
                    trans->rollback();
                    ++rolled_back;
                } else {
                    trans->commit();
                    ++committed;
                }
            } catch (...) {
                ++errors;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
    EXPECT_GT(committed.load(), 0);
}

// Test 5: Connection reuse safety
TEST_F(DatabaseThreadSafetyTest, ConnectionReuseSafety) {
    connection_pool pool("test_db", 8);

    const int num_threads = 15;
    const int cycles_per_thread = 100;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < cycles_per_thread; ++j) {
                try {
                    auto conn = pool.acquire_connection();
                    if (conn) {
                        conn->execute("SELECT 1");
                        pool.release_connection(std::move(conn));
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 6: Prepared statement concurrent execution
TEST_F(DatabaseThreadSafetyTest, PreparedStatementConcurrent) {
    auto db = database_manager::create("test_db");
    auto stmt = db->prepare("SELECT ? + ?");

    const int num_threads = 12;
    const int executions_per_thread = 200;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < executions_per_thread; ++j) {
                try {
                    auto result = stmt->execute(thread_id, j);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 7: Batch operations concurrent
TEST_F(DatabaseThreadSafetyTest, BatchOperationsConcurrent) {
    auto db = database_manager::create("test_db");

    const int num_batches = 50;
    const int items_per_batch = 100;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_batches; ++i) {
        threads.emplace_back([&, batch_id = i]() {
            try {
                auto batch = db->create_batch();

                for (int j = 0; j < items_per_batch; ++j) {
                    batch->add("INSERT INTO test VALUES (" + std::to_string(batch_id * 1000 + j) + ")");
                }

                batch->execute();
            } catch (...) {
                ++errors;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 8: Connection timeout during high load
TEST_F(DatabaseThreadSafetyTest, ConnectionTimeoutHighLoad) {
    connection_pool pool("test_db", 3);

    const int num_threads = 20;
    std::atomic<int> timeouts{0};
    std::atomic<int> successes{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            auto conn = pool.acquire_connection(50ms);
            if (conn) {
                ++successes;
                std::this_thread::sleep_for(100ms);
                pool.release_connection(std::move(conn));
            } else {
                ++timeouts;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(timeouts.load(), 0);
    EXPECT_GT(successes.load(), 0);
}

// Test 9: Result set iteration concurrent
TEST_F(DatabaseThreadSafetyTest, ResultSetIterationConcurrent) {
    auto db = database_manager::create("test_db");
    auto result = db->execute("SELECT * FROM large_table");

    const int num_readers = 10;
    std::atomic<int> rows_read{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            try {
                auto cursor = result->create_cursor();
                while (cursor->next()) {
                    ++rows_read;
                }
            } catch (...) {
                ++errors;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 10: Memory safety - no leaks during concurrent database operations
TEST_F(DatabaseThreadSafetyTest, MemorySafetyTest) {
    const int num_iterations = 30;
    const int threads_per_iteration = 10;
    const int operations_per_thread = 50;

    std::atomic<int> total_errors{0};

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        connection_pool pool("test_db", 5);
        std::vector<std::thread> threads;

        for (int i = 0; i < threads_per_iteration; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        auto conn = pool.acquire_connection();
                        if (conn) {
                            conn->execute("SELECT 1");
                            pool.release_connection(std::move(conn));
                        }
                    } catch (...) {
                        ++total_errors;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Pool destructor called here
    }

    EXPECT_EQ(total_errors.load(), 0);
}
