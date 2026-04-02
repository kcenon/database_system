// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "mock_database.h"
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>

namespace database::testing {

/**
 * @class mock_connection_pool
 * @brief Mock connection pool for testing pool-related functionality
 *
 * Features:
 * - Configurable pool size
 * - Simulated connection acquisition delays
 * - Connection leak detection
 * - Pool exhaustion simulation
 */
class mock_connection_pool {
public:
    struct config {
        size_t min_size = 2;
        size_t max_size = 10;
        std::chrono::milliseconds acquire_timeout{5000};
        bool simulate_slow_acquire = false;
        std::chrono::milliseconds slow_acquire_delay{100};
    };

    explicit mock_connection_pool(const config& cfg = {});
    ~mock_connection_pool();

    /**
     * @brief Acquire a connection from the pool
     * @return Pointer to mock_database, or nullptr if pool is exhausted
     */
    mock_database* acquire();

    /**
     * @brief Acquire a connection with timeout
     * @param timeout Maximum time to wait
     * @return Pointer to mock_database, or nullptr if timeout
     */
    mock_database* acquire(std::chrono::milliseconds timeout);

    /**
     * @brief Release a connection back to the pool
     * @param conn Connection to release
     */
    void release(mock_database* conn);

    /**
     * @brief Get current pool statistics
     */
    struct stats {
        size_t total_connections;
        size_t available_connections;
        size_t in_use_connections;
        size_t total_acquisitions;
        size_t total_releases;
        size_t acquisition_timeouts;
        size_t leaked_connections;
    };
    stats get_stats() const;

    /**
     * @brief Simulate pool exhaustion
     */
    void simulate_exhaustion();

    /**
     * @brief Reset pool to normal operation
     */
    void reset();

    /**
     * @brief Configure all connections with same expectation
     */
    void configure_all(std::function<void(mock_database&)> configurator);

private:
    config config_;
    std::vector<std::unique_ptr<mock_database>> connections_;
    std::queue<mock_database*> available_;
    std::set<mock_database*> in_use_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> exhausted_{false};

    // Statistics
    std::atomic<size_t> total_acquisitions_{0};
    std::atomic<size_t> total_releases_{0};
    std::atomic<size_t> acquisition_timeouts_{0};

    void create_connection();
};

/**
 * @class scoped_connection
 * @brief RAII wrapper for pool connections
 */
class scoped_connection {
public:
    scoped_connection(mock_connection_pool& pool)
        : pool_(pool), conn_(pool.acquire())
    {}

    scoped_connection(mock_connection_pool& pool, std::chrono::milliseconds timeout)
        : pool_(pool), conn_(pool.acquire(timeout))
    {}

    ~scoped_connection() {
        if (conn_) {
            pool_.release(conn_);
        }
    }

    // Non-copyable
    scoped_connection(const scoped_connection&) = delete;
    scoped_connection& operator=(const scoped_connection&) = delete;

    // Movable
    scoped_connection(scoped_connection&& other) noexcept
        : pool_(other.pool_), conn_(other.conn_)
    {
        other.conn_ = nullptr;
    }

    mock_database* get() { return conn_; }
    mock_database* operator->() { return conn_; }
    mock_database& operator*() { return *conn_; }
    explicit operator bool() const { return conn_ != nullptr; }

private:
    mock_connection_pool& pool_;
    mock_database* conn_;
};

// Implementation
inline mock_connection_pool::mock_connection_pool(const config& cfg)
    : config_(cfg)
{
    for (size_t i = 0; i < config_.min_size; ++i) {
        create_connection();
    }
}

inline mock_connection_pool::~mock_connection_pool() = default;

inline void mock_connection_pool::create_connection() {
    auto conn = std::make_unique<mock_database>();
    available_.push(conn.get());
    connections_.push_back(std::move(conn));
}

inline mock_database* mock_connection_pool::acquire() {
    return acquire(config_.acquire_timeout);
}

inline mock_database* mock_connection_pool::acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (exhausted_) {
        ++acquisition_timeouts_;
        return nullptr;
    }

    // Wait for available connection or create new one
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (available_.empty()) {
        if (connections_.size() < config_.max_size) {
            create_connection();
            break;
        }

        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            ++acquisition_timeouts_;
            return nullptr;
        }

        if (exhausted_) {
            ++acquisition_timeouts_;
            return nullptr;
        }
    }

    if (available_.empty()) {
        ++acquisition_timeouts_;
        return nullptr;
    }

    auto* conn = available_.front();
    available_.pop();
    in_use_.insert(conn);
    ++total_acquisitions_;

    // Simulate slow acquisition if configured
    if (config_.simulate_slow_acquire) {
        lock.unlock();
        std::this_thread::sleep_for(config_.slow_acquire_delay);
    }

    return conn;
}

inline void mock_connection_pool::release(mock_database* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_use_.erase(conn)) {
        available_.push(conn);
        ++total_releases_;
        cv_.notify_one();
    }
}

inline mock_connection_pool::stats mock_connection_pool::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        connections_.size(),
        available_.size(),
        in_use_.size(),
        total_acquisitions_.load(),
        total_releases_.load(),
        acquisition_timeouts_.load(),
        total_acquisitions_.load() - total_releases_.load()
    };
}

inline void mock_connection_pool::simulate_exhaustion() {
    exhausted_ = true;
}

inline void mock_connection_pool::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    exhausted_ = false;
    // Return all in-use to available
    for (auto* conn : in_use_) {
        available_.push(conn);
    }
    in_use_.clear();
}

inline void mock_connection_pool::configure_all(std::function<void(mock_database&)> configurator) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& conn : connections_) {
        configurator(*conn);
    }
}

} // namespace database::testing
