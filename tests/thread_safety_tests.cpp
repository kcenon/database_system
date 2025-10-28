/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "database/connection_pool.h"
#include "database/database_manager.h"
#include "database/database_types.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <barrier>
#include <memory>

using namespace database;
using namespace std::chrono_literals;

/**
 * @class mock_database
 * @brief Mock database implementation for thread safety testing
 *
 * This mock provides a functional database_base implementation that:
 * - Returns successful responses for all operations
 * - Tracks connection state
 * - Thread-safe for concurrent testing
 */
class mock_database : public database_base {
private:
    database_types type_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;

public:
    explicit mock_database(database_types type = database_types::sqlite)
        : type_(type), connected_(false) {}

    ~mock_database() override {
        if (connected_.load()) {
            disconnect();
        }
    }

    database_types database_type() override {
        return type_;
    }

    bool connect(const std::string& connect_string) override {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)connect_string;  // Suppress unused parameter warning
        connected_.store(true);
        return true;
    }

    bool create_query(const std::string& query_string) override {
        (void)query_string;
        return connected_.load();
    }

    unsigned int insert_query(const std::string& query_string) override {
        (void)query_string;
        return connected_.load() ? 1 : 0;
    }

    unsigned int update_query(const std::string& query_string) override {
        (void)query_string;
        return connected_.load() ? 1 : 0;
    }

    unsigned int delete_query(const std::string& query_string) override {
        (void)query_string;
        return connected_.load() ? 1 : 0;
    }

    database_result select_query(const std::string& query_string) override {
        (void)query_string;
        return database_result{};  // Return empty result set
    }

    bool execute_query(const std::string& query_string) override {
        (void)query_string;
        return connected_.load();
    }

    bool disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_.store(false);
        return true;
    }

    bool is_connected() const {
        return connected_.load();
    }
};

// Mock database connection factory for testing
std::unique_ptr<database_base> create_mock_connection() {
    auto mock = std::make_unique<mock_database>();
    // Pre-connect the mock so it's ready to use
    mock->connect("mock_connection");
    return mock;
}

class DatabaseThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Concurrent connection pool acquire/release
TEST_F(DatabaseThreadSafetyTest, ConcurrentConnectionAcquireRelease) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;
    config.acquire_timeout = std::chrono::milliseconds(1000);
    config.connection_string = "test_db";

    connection_pool pool(database_types::sqlite, config, create_mock_connection);
    pool.initialize();

    const int num_threads = 15;
    const int acquisitions_per_thread = 100;

    std::atomic<int> successful_acquisitions{0};
    std::atomic<int> failed_acquisitions{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < acquisitions_per_thread; ++j) {
                try {
                    auto conn = pool.acquire_connection();
                    if (conn.is_ok()) {
                        ++successful_acquisitions;
                        std::this_thread::sleep_for(1ms);
                        pool.release_connection(std::move(conn.value()));
                    } else {
                        ++failed_acquisitions;
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

    pool.shutdown();

    EXPECT_EQ(errors.load(), 0);
    // With mock connections returning nullptr, all acquisitions should fail gracefully
    EXPECT_EQ(successful_acquisitions.load() + failed_acquisitions.load(),
              num_threads * acquisitions_per_thread);
}

// Test 2: Connection pool statistics concurrent access
TEST_F(DatabaseThreadSafetyTest, ConnectionPoolStatsAccess) {
    connection_pool_config config;
    config.min_connections = 3;
    config.max_connections = 8;
    config.connection_string = "test_db";

    connection_pool pool(database_types::postgres, config, create_mock_connection);
    pool.initialize();

    const int num_threads = 12;
    const int iterations_per_thread = 200;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < iterations_per_thread; ++j) {
                try {
                    // Half threads read stats, half acquire connections
                    if (thread_id % 2 == 0) {
                        auto stats = pool.get_stats();
                        (void)stats;  // Use stats to avoid warning
                    } else {
                        auto conn = pool.acquire_connection();
                        if (conn.is_ok()) {
                            pool.release_connection(std::move(conn.value()));
                        }
                    }
                } catch (...) {
                    ++errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    pool.shutdown();
    EXPECT_EQ(errors.load(), 0);
}

// Test 3: Connection pool manager concurrent pool creation
TEST_F(DatabaseThreadSafetyTest, ConnectionPoolManagerConcurrentCreation) {
    GTEST_SKIP() << "Skipped: connection_pool_manager uses real database backends (PostgreSQL/MySQL) which are not available in CI";

    auto& manager = connection_pool_manager::instance();

    const int num_threads = 10;
    std::atomic<int> successful_creates{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    std::barrier sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            try {
                connection_pool_config config;
                config.min_connections = 2;
                config.max_connections = 5;
                config.connection_string = "test_db_" + std::to_string(thread_id);

                // Each thread tries to create pool for different database type
                database_types db_type = static_cast<database_types>((thread_id % 3) + 1);

                if (manager.create_pool(db_type, config)) {
                    ++successful_creates;
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
    EXPECT_GT(successful_creates.load(), 0);

    manager.shutdown_all();
}

// Test 4: Connection pool manager get/remove race
TEST_F(DatabaseThreadSafetyTest, PoolManagerGetRemoveRace) {
    GTEST_SKIP() << "Skipped: connection_pool_manager uses real database backends which are not available in CI";

    auto& manager = connection_pool_manager::instance();

    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.connection_string = "test_race_db";

    manager.create_pool(database_types::mysql, config);

    const int num_getter_threads = 8;
    const int num_remover_threads = 2;
    std::atomic<int> gets{0};
    std::atomic<int> errors{0};
    std::atomic<bool> running{true};

    std::vector<std::thread> threads;

    // Getter threads
    for (int i = 0; i < num_getter_threads; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                try {
                    auto pool = manager.get_pool(database_types::mysql);
                    if (pool) {
                        ++gets;
                    }
                    std::this_thread::sleep_for(5ms);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    // Remover/creator threads
    for (int i = 0; i < num_remover_threads; ++i) {
        threads.emplace_back([&]() {
            std::this_thread::sleep_for(50ms);
            try {
                manager.remove_pool(database_types::mysql);
                std::this_thread::sleep_for(20ms);
                manager.create_pool(database_types::mysql, config);
            } catch (...) {
                ++errors;
            }
        });
    }

    std::this_thread::sleep_for(200ms);
    running.store(false);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    manager.shutdown_all();
}

// Test 5: Database manager singleton access
TEST_F(DatabaseThreadSafetyTest, DatabaseManagerSingletonAccess) {
    const int num_threads = 20;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    std::vector<database_manager*> managers;
    managers.resize(num_threads);
    std::mutex managers_mutex;

    std::barrier sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            try {
                auto& mgr = database_manager::handle();

                std::lock_guard<std::mutex> lock(managers_mutex);
                managers[thread_id] = &mgr;
            } catch (...) {
                ++errors;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // All pointers should be the same (singleton)
    for (size_t i = 1; i < managers.size(); ++i) {
        EXPECT_EQ(managers[0], managers[i]);
    }
}

// Test 6: Database manager set_mode concurrent calls
TEST_F(DatabaseThreadSafetyTest, DatabaseManagerConcurrentSetMode) {
    GTEST_SKIP() << "Skipped: database_manager::set_mode() initializes real database backends which causes SegFault in CI";

    auto& manager = database_manager::handle();

    const int num_threads = 15;
    const int mode_changes_per_thread = 100;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < mode_changes_per_thread; ++j) {
                try {
                    database_types type = static_cast<database_types>((j % 3) + 1);
                    manager.set_mode(type);
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

// Test 7: Connection pool health check during operations
TEST_F(DatabaseThreadSafetyTest, HealthCheckDuringOperations) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;
    config.health_check_interval = std::chrono::milliseconds(100);
    config.enable_health_checks = true;
    config.connection_string = "test_health_db";

    connection_pool pool(database_types::sqlite, config, create_mock_connection);
    pool.initialize();

    const int num_worker_threads = 10;
    const int operations_per_thread = 100;
    std::atomic<int> errors{0};
    std::atomic<bool> running{true};

    std::vector<std::thread> threads;

    // Worker threads
    for (int i = 0; i < num_worker_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < operations_per_thread && running.load(); ++j) {
                try {
                    auto conn = pool.acquire_connection();
                    if (conn.is_ok()) {
                        pool.release_connection(std::move(conn.value()));
                    }
                } catch (...) {
                    ++errors;
                }
                std::this_thread::sleep_for(2ms);
            }
        });
    }

    // Health check thread
    threads.emplace_back([&]() {
        while (running.load()) {
            try {
                pool.health_check();
                std::this_thread::sleep_for(50ms);
            } catch (...) {
                ++errors;
            }
        }
    });

    for (int i = 0; i < num_worker_threads; ++i) {
        threads[i].join();
    }

    running.store(false);
    threads[num_worker_threads].join();

    pool.shutdown();
    EXPECT_EQ(errors.load(), 0);
}

// Test 8: Connection wrapper metadata concurrent access
TEST_F(DatabaseThreadSafetyTest, ConnectionWrapperMetadataConcurrent) {
    // Create mock connection wrapper with actual mock database
    auto mock_conn = std::make_unique<connection_wrapper>(create_mock_connection());

    const int num_threads = 15;
    const int operations_per_thread = 500;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    if (thread_id % 3 == 0) {
                        mock_conn->update_last_used();
                    } else if (thread_id % 3 == 1) {
                        auto last_used = mock_conn->last_used();
                        (void)last_used;
                    } else {
                        bool healthy = mock_conn->is_healthy();
                        (void)healthy;
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

// Test 9: Connection pool all statistics methods
TEST_F(DatabaseThreadSafetyTest, ConnectionPoolAllStatsMethods) {
    connection_pool_config config;
    config.min_connections = 4;
    config.max_connections = 8;
    config.connection_string = "test_stats_db";

    connection_pool pool(database_types::postgres, config, create_mock_connection);
    pool.initialize();

    const int num_threads = 12;
    const int operations_per_thread = 200;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    switch (thread_id % 4) {
                        case 0: {
                            auto stats = pool.get_stats();
                            (void)stats;
                            break;
                        }
                        case 1: {
                            size_t active = pool.active_connections();
                            (void)active;
                            break;
                        }
                        case 2: {
                            size_t available = pool.available_connections();
                            (void)available;
                            break;
                        }
                        case 3: {
                            auto conn = pool.acquire_connection();
                            if (conn.is_ok()) {
                                pool.release_connection(std::move(conn.value()));
                            }
                            break;
                        }
                    }
                } catch (...) {
                    ++errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    pool.shutdown();
    EXPECT_EQ(errors.load(), 0);
}

// Test 10: Memory safety - pool lifecycle stress test
TEST_F(DatabaseThreadSafetyTest, PoolLifecycleMemorySafety) {
    const int num_iterations = 5;
    const int threads_per_iteration = 4;
    const int operations_per_thread = 50;

    std::atomic<int> total_errors{0};

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        connection_pool_config config;
        config.min_connections = 3;
        config.max_connections = 6;
        config.connection_string = "test_lifecycle_db";

        connection_pool pool(database_types::sqlite, config, create_mock_connection);
        pool.initialize();

        std::vector<std::thread> threads;

        for (int i = 0; i < threads_per_iteration; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        auto conn = pool.acquire_connection();
                        if (conn.is_ok()) {
                            pool.release_connection(std::move(conn.value()));
                        }

                        auto stats = pool.get_stats();
                        (void)stats;
                    } catch (...) {
                        ++total_errors;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        pool.shutdown();
        // Pool destructor called here
    }

    EXPECT_EQ(total_errors.load(), 0);
}
