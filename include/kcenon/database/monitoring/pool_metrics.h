// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <map>
#include <mutex>

namespace database::monitoring {

/**
 * @struct pool_metrics
 * @brief Performance metrics for connection pools
 *
 * Tracks key performance indicators for database connection management:
 * - Connection acquisition latency
 * - Queue wait times
 * - Throughput rates
 * - Priority-based statistics
 */
struct pool_metrics {
    // Connection statistics
    std::atomic<uint64_t> total_acquisitions{0};
    std::atomic<uint64_t> successful_acquisitions{0};
    std::atomic<uint64_t> failed_acquisitions{0};
    std::atomic<uint64_t> timeouts{0};

    // Timing statistics (microseconds)
    std::atomic<uint64_t> total_wait_time_us{0};
    std::atomic<uint64_t> min_wait_time_us{UINT64_MAX};
    std::atomic<uint64_t> max_wait_time_us{0};

    // Current state
    std::atomic<uint64_t> current_active{0};
    std::atomic<uint64_t> current_queued{0};
    std::atomic<uint64_t> peak_active{0};
    std::atomic<uint64_t> peak_queued{0};

    // Health check statistics
    std::atomic<uint64_t> health_checks_performed{0};
    std::atomic<uint64_t> unhealthy_connections_removed{0};

    /**
     * @brief Record a connection acquisition
     * @param wait_time_us Wait time in microseconds
     * @param success Whether acquisition was successful
     */
    void record_acquisition(uint64_t wait_time_us, bool success) {
        total_acquisitions.fetch_add(1, std::memory_order_relaxed);

        if (success) {
            successful_acquisitions.fetch_add(1, std::memory_order_relaxed);

            // Update timing statistics
            total_wait_time_us.fetch_add(wait_time_us, std::memory_order_relaxed);

            // Update min/max (requires atomic compare-exchange)
            uint64_t current_min = min_wait_time_us.load(std::memory_order_relaxed);
            while (wait_time_us < current_min &&
                   !min_wait_time_us.compare_exchange_weak(current_min, wait_time_us,
                                                           std::memory_order_relaxed)) {}

            uint64_t current_max = max_wait_time_us.load(std::memory_order_relaxed);
            while (wait_time_us > current_max &&
                   !max_wait_time_us.compare_exchange_weak(current_max, wait_time_us,
                                                           std::memory_order_relaxed)) {}
        } else {
            failed_acquisitions.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Record a timeout event
     */
    void record_timeout() {
        timeouts.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Update current active connection count
     * @param delta Change in active connections (+1 for acquire, -1 for release)
     */
    void update_active(int delta) {
        uint64_t new_active = current_active.fetch_add(delta, std::memory_order_relaxed) + delta;

        // Update peak if necessary
        uint64_t current_peak = peak_active.load(std::memory_order_relaxed);
        while (new_active > current_peak &&
               !peak_active.compare_exchange_weak(current_peak, new_active,
                                                  std::memory_order_relaxed)) {}
    }

    /**
     * @brief Update current queued request count
     * @param delta Change in queued requests
     */
    void update_queued(int delta) {
        uint64_t new_queued = current_queued.fetch_add(delta, std::memory_order_relaxed) + delta;

        // Update peak if necessary
        uint64_t current_peak = peak_queued.load(std::memory_order_relaxed);
        while (new_queued > current_peak &&
               !peak_queued.compare_exchange_weak(current_peak, new_queued,
                                                  std::memory_order_relaxed)) {}
    }

    /**
     * @brief Record a health check operation
     * @param removed_connections Number of unhealthy connections removed
     */
    void record_health_check(uint64_t removed_connections = 0) {
        health_checks_performed.fetch_add(1, std::memory_order_relaxed);
        if (removed_connections > 0) {
            unhealthy_connections_removed.fetch_add(removed_connections, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Calculate average wait time
     * @return Average wait time in microseconds
     */
    double average_wait_time_us() const {
        uint64_t total = total_acquisitions.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;

        uint64_t total_wait = total_wait_time_us.load(std::memory_order_relaxed);
        return static_cast<double>(total_wait) / static_cast<double>(total);
    }

    /**
     * @brief Calculate success rate
     * @return Success rate as percentage (0.0 - 100.0)
     */
    double success_rate() const {
        uint64_t total = total_acquisitions.load(std::memory_order_relaxed);
        if (total == 0) return 100.0;

        uint64_t successful = successful_acquisitions.load(std::memory_order_relaxed);
        return (static_cast<double>(successful) / static_cast<double>(total)) * 100.0;
    }

    /**
     * @brief Reset all metrics
     */
    void reset() {
        total_acquisitions.store(0, std::memory_order_relaxed);
        successful_acquisitions.store(0, std::memory_order_relaxed);
        failed_acquisitions.store(0, std::memory_order_relaxed);
        timeouts.store(0, std::memory_order_relaxed);

        total_wait_time_us.store(0, std::memory_order_relaxed);
        min_wait_time_us.store(UINT64_MAX, std::memory_order_relaxed);
        max_wait_time_us.store(0, std::memory_order_relaxed);

        // Don't reset current_active and current_queued (they reflect current state)
        peak_active.store(current_active.load(std::memory_order_relaxed), std::memory_order_relaxed);
        peak_queued.store(current_queued.load(std::memory_order_relaxed), std::memory_order_relaxed);

        health_checks_performed.store(0, std::memory_order_relaxed);
        unhealthy_connections_removed.store(0, std::memory_order_relaxed);
    }
};

#ifdef USE_THREAD_SYSTEM
/**
 * @struct priority_metrics
 * @brief Priority-specific metrics for typed thread pools
 *
 * Extends pool_metrics with priority-level statistics.
 * Tracks performance per connection_priority level.
 */
template<typename PriorityType>
struct priority_metrics : public pool_metrics {
    using priority_type = PriorityType;

    // Per-priority statistics
    std::map<PriorityType, std::atomic<uint64_t>> acquisitions_by_priority;
    std::map<PriorityType, std::atomic<uint64_t>> wait_time_by_priority;

    mutable std::mutex map_mutex;  // Protects map modifications

    /**
     * @brief Record acquisition with priority tracking
     * @param priority Request priority
     * @param wait_time_us Wait time in microseconds
     * @param success Whether acquisition was successful
     */
    void record_acquisition_with_priority(PriorityType priority, uint64_t wait_time_us, bool success) {
        // Record base metrics
        record_acquisition(wait_time_us, success);

        if (success) {
            // Update priority-specific metrics
            std::lock_guard<std::mutex> lock(map_mutex);

            // Initialize if first time seeing this priority
            if (acquisitions_by_priority.find(priority) == acquisitions_by_priority.end()) {
                acquisitions_by_priority[priority].store(0, std::memory_order_relaxed);
                wait_time_by_priority[priority].store(0, std::memory_order_relaxed);
            }

            acquisitions_by_priority[priority].fetch_add(1, std::memory_order_relaxed);
            wait_time_by_priority[priority].fetch_add(wait_time_us, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Get average wait time for a specific priority
     * @param priority Priority level to query
     * @return Average wait time in microseconds, or 0 if no data
     */
    double average_wait_time_for_priority(PriorityType priority) const {
        std::lock_guard<std::mutex> lock(map_mutex);

        auto acq_it = acquisitions_by_priority.find(priority);
        auto wait_it = wait_time_by_priority.find(priority);

        if (acq_it == acquisitions_by_priority.end() || wait_it == wait_time_by_priority.end()) {
            return 0.0;
        }

        uint64_t count = acq_it->second.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;

        uint64_t total_wait = wait_it->second.load(std::memory_order_relaxed);
        return static_cast<double>(total_wait) / static_cast<double>(count);
    }

    /**
     * @brief Reset all metrics including priority-specific data
     */
    void reset_all() {
        pool_metrics::reset();

        std::lock_guard<std::mutex> lock(map_mutex);
        for (auto& [priority, counter] : acquisitions_by_priority) {
            counter.store(0, std::memory_order_relaxed);
        }
        for (auto& [priority, wait_time] : wait_time_by_priority) {
            wait_time.store(0, std::memory_order_relaxed);
        }
    }
};
#endif // USE_THREAD_SYSTEM

} // namespace database::monitoring
