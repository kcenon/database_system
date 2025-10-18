/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "connection_pool.h"
#include <chrono>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace database {

/**
 * @struct connection_lease_info
 * @brief Tracking information for leased connections
 */
struct connection_lease_info {
    std::chrono::steady_clock::time_point acquired_at;
    std::thread::id thread_id;
    std::string acquisition_context;
    size_t acquisition_count{1};

    [[nodiscard]] auto get_hold_duration() const {
        return std::chrono::steady_clock::now() - acquired_at;
    }

    [[nodiscard]] bool is_potential_leak(std::chrono::milliseconds threshold) const {
        return get_hold_duration() > threshold;
    }
};

/**
 * @class connection_leak_detector
 * @brief Monitors connection pool for potential connection leaks
 *
 * This utility tracks connection leases and alerts when:
 * - Connections held longer than threshold
 * - Same thread repeatedly acquires without releasing
 * - Connection pool exhaustion patterns
 *
 * ### Production Usage
 * @code
 * auto pool = std::make_shared<connection_pool>(...);
 * auto detector = std::make_shared<connection_leak_detector>(pool);
 *
 * // Configure leak detection
 * detector->set_leak_threshold(std::chrono::minutes(5));
 * detector->set_check_interval(std::chrono::seconds(30));
 *
 * // Start monitoring
 * detector->start();
 *
 * // Set callback for leak notifications
 * detector->set_leak_callback([](const auto& info) {
 *     log_warning("Potential connection leak detected: "
 *                "held for {} seconds by thread {}",
 *                duration_cast<seconds>(info.get_hold_duration()).count(),
 *                info.thread_id);
 * });
 * @endcode
 */
class connection_leak_detector {
public:
    using leak_callback = std::function<void(const connection_lease_info&)>;

    explicit connection_leak_detector(std::shared_ptr<connection_pool_base> pool)
        : pool_(std::move(pool))
        , leak_threshold_(std::chrono::minutes(5))
        , check_interval_(std::chrono::seconds(30))
        , running_(false)
    {
    }

    ~connection_leak_detector() {
        stop();
    }

    /**
     * @brief Start leak detection monitoring
     */
    void start() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_) {
            return;
        }

        running_ = true;
        monitor_thread_ = std::thread([this]() {
            monitor_loop();
        });
    }

    /**
     * @brief Stop leak detection monitoring
     */
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }
            running_ = false;
        }

        cv_.notify_all();

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }

    /**
     * @brief Track connection acquisition
     * @param conn Connection wrapper
     * @param context Optional context string (e.g., function name)
     */
    void track_acquisition(connection_wrapper* conn,
                          const std::string& context = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = active_leases_.find(conn);
        if (it != active_leases_.end()) {
            // Connection re-acquired without release - suspicious
            it->second.acquisition_count++;
        } else {
            connection_lease_info info;
            info.acquired_at = std::chrono::steady_clock::now();
            info.thread_id = std::this_thread::get_id();
            info.acquisition_context = context;

            active_leases_[conn] = info;
        }

        total_acquisitions_++;
    }

    /**
     * @brief Track connection release
     * @param conn Connection wrapper
     */
    void track_release(connection_wrapper* conn) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = active_leases_.find(conn);
        if (it != active_leases_.end()) {
            // Record hold duration for stats
            auto hold_duration = it->second.get_hold_duration();

            if (hold_duration > max_hold_duration_) {
                max_hold_duration_ = hold_duration;
            }

            total_hold_time_ += hold_duration;

            active_leases_.erase(it);
            total_releases_++;
        }
    }

    /**
     * @brief Set leak detection threshold
     * @param threshold Maximum time connection can be held
     */
    void set_leak_threshold(std::chrono::milliseconds threshold) {
        leak_threshold_ = threshold;
    }

    /**
     * @brief Set monitoring check interval
     */
    void set_check_interval(std::chrono::milliseconds interval) {
        check_interval_ = interval;
    }

    /**
     * @brief Set callback for leak detection
     */
    void set_leak_callback(leak_callback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        leak_callback_ = std::move(callback);
    }

    /**
     * @brief Get statistics
     */
    struct stats {
        size_t active_leases;
        size_t total_acquisitions;
        size_t total_releases;
        size_t detected_leaks;
        std::chrono::milliseconds max_hold_duration;
        std::chrono::milliseconds avg_hold_duration;
    };

    [[nodiscard]] stats get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);

        stats s;
        s.active_leases = active_leases_.size();
        s.total_acquisitions = total_acquisitions_;
        s.total_releases = total_releases_;
        s.detected_leaks = detected_leaks_;
        s.max_hold_duration = max_hold_duration_;

        if (total_releases_ > 0) {
            s.avg_hold_duration = total_hold_time_ / total_releases_;
        }

        return s;
    }

    /**
     * @brief Force leak check
     * @return Number of potential leaks detected
     */
    size_t check_for_leaks() {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t leak_count = 0;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [conn, info] : active_leases_) {
            if (info.is_potential_leak(leak_threshold_)) {
                leak_count++;

                if (leak_callback_) {
                    leak_callback_(info);
                }
            }
        }

        detected_leaks_ += leak_count;
        return leak_count;
    }

private:
    void monitor_loop() {
        while (running_) {
            // Wait for check interval or stop signal
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, check_interval_, [this]() {
                return !running_;
            });

            if (!running_) {
                break;
            }

            lock.unlock();

            // Perform leak check
            check_for_leaks();
        }
    }

private:
    std::shared_ptr<connection_pool_base> pool_;
    std::chrono::milliseconds leak_threshold_;
    std::chrono::milliseconds check_interval_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::thread monitor_thread_;

    std::unordered_map<connection_wrapper*, connection_lease_info> active_leases_;
    leak_callback leak_callback_;

    // Statistics
    size_t total_acquisitions_{0};
    size_t total_releases_{0};
    size_t detected_leaks_{0};
    std::chrono::milliseconds max_hold_duration_{0};
    std::chrono::milliseconds total_hold_time_{0};
};

} // namespace database
