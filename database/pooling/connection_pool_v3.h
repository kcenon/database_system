/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

#include "../connection_pool.h"
#include "../database_base.h"
#include "../database_types.h"
#include "../monitoring/pool_metrics.h"
#include "../pooling/connection_pool_v2.h"  // Reuse priority enum
#include <memory>
#include <chrono>
#include <future>
#include <functional>

#include <kcenon/thread/core/typed_thread_pool.h>
#include <kcenon/thread/core/typed_thread_worker.h>
#include <kcenon/thread/core/cancellation_token.h>
#include <kcenon/thread/core/error_handling.h>

// Adaptive queue implementation (from thread_system)
#include <kcenon/thread/impl/typed_pool/adaptive_typed_job_queue.h>

namespace database::pooling {

/**
 * @class connection_acquisition_job
 * @brief Typed job for adaptive priority-based connection acquisition in v3
 *
 * This job implements connection acquisition logic integrated with
 * thread_system's adaptive_typed_job_queue for optimal performance
 * under varying load conditions.
 */
class connection_acquisition_job : public kcenon::thread::typed_job_t<connection_priority> {
public:
    using completion_callback = std::function<void(Result<std::shared_ptr<connection_wrapper>>)>;

    /**
     * @brief Constructs a connection acquisition job
     * @param priority Priority level for this request
     * @param pool_ref Reference to the underlying connection pool
     * @param callback Callback to invoke with the result
     */
    explicit connection_acquisition_job(
        connection_priority priority,
        std::shared_ptr<connection_pool_base> pool_ref,
        completion_callback callback)
        : kcenon::thread::typed_job_t<connection_priority>(priority, "connection_acquisition_v3")
        , pool_ref_(std::move(pool_ref))
        , callback_(std::move(callback))
    {}

    kcenon::thread::result_void do_work() override {
        try {
            // Acquire connection from the underlying pool
            auto result = pool_ref_->acquire_connection();

            // Invoke callback with result
            if (callback_) {
                callback_(std::move(result));
            }

            return kcenon::thread::result_void{};
        } catch (const std::exception& e) {
            // On exception, invoke callback with error
            if (callback_) {
                callback_(error_info{-599, std::string("Exception in connection acquisition: ") + e.what(), "connection_pool_v3"});
            }

            return kcenon::thread::error{
                kcenon::thread::error_code::job_execution_failed,
                std::string("Exception in connection_acquisition_job: ") + e.what()
            };
        }
    }

private:
    std::shared_ptr<connection_pool_base> pool_ref_;
    completion_callback callback_;
};

/**
 * @class connection_pool_v3
 * @brief Next-generation connection pool with adaptive queue and cancellation support
 *
 * connection_pool_v3 is the third generation connection pool that builds upon v2 with:
 * - **Adaptive Job Queue**: Automatically switches between mutex-based and lock-free
 *   implementations based on runtime contention metrics (4x-7.7x performance improvement)
 * - **Cancellation Token**: Graceful shutdown with cooperative cancellation
 * - **Enhanced Metrics**: Detailed performance monitoring with queue-specific metrics
 * - **Ultra-Low Latency**: Target < 100ns connection acquisition latency (65x improvement)
 * - **High Throughput**: Target > 1M ops/s (leveraging adaptive queue)
 *
 * ### Architecture Improvements from v2
 * 1. **Adaptive Queue Strategy**: Dynamically selects optimal queue implementation
 *    - Low contention: Mutex-based queue (lower overhead)
 *    - High contention: Lock-free queue (better scalability)
 *    - Automatic evaluation and switching every 5 seconds
 * 2. **Cooperative Cancellation**: Clean shutdown without forceful thread termination
 * 3. **Performance Metrics**: Track queue strategy switches, contention ratios, and latencies
 *
 * ### Performance Targets (from IMPROVEMENT_PLAN.md)
 * - Connection acquisition latency: < 100ns
 * - Throughput: > 1M ops/s
 * - High-load performance: 4x-7.7x improvement over v2
 * - Overhead: Minimal due to adaptive queue strategy
 *
 * ### Priority Levels (from v2)
 * - CRITICAL: Time-critical operations that cannot be delayed
 * - TRANSACTION: Active transactions requiring immediate response
 * - NORMAL_QUERY: Standard database queries (default priority)
 * - HEALTH_CHECK: Background health monitoring (lowest priority)
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called from multiple threads concurrently.
 *
 * ### Migration from v2
 * @code
 * // v2 code (still works)
 * connection_pool_v2 pool_v2(db_type, config, factory);
 * auto future = pool_v2.acquire_connection(connection_priority::CRITICAL);
 *
 * // v3 code (drop-in replacement with better performance)
 * connection_pool_v3 pool_v3(db_type, config, factory);
 * auto future = pool_v3.acquire_connection(connection_priority::CRITICAL);
 *
 * // New v3 feature: cancellation
 * pool_v3.request_shutdown();  // Graceful shutdown
 * pool_v3.shutdown();           // Wait for completion
 * @endcode
 *
 * ### Example: Monitoring Adaptive Queue Performance
 * @code
 * auto metrics = pool_v3.get_metrics();
 * std::cout << "Current queue type: " << pool_v3.get_current_queue_type() << "\n";
 * std::cout << "Queue switches: " << pool_v3.get_queue_switch_count() << "\n";
 * std::cout << "Contention ratio: " << pool_v3.get_contention_ratio() << "%\n";
 * @endcode
 */
class connection_pool_v3 {
public:
    /**
     * @brief Constructs connection pool v3 with adaptive queue
     * @param db_type Database type for this pool
     * @param config Pool configuration
     * @param factory Function to create new database connections
     * @param thread_count Number of worker threads (default: hardware_concurrency)
     * @param queue_strategy Initial queue strategy (default: AUTO_DETECT for best performance)
     *
     * ### Queue Strategies
     * - AUTO_DETECT: Automatically choose best strategy based on initial load
     * - FORCE_LEGACY: Always use mutex-based queue (predictable latency)
     * - FORCE_LOCKFREE: Always use lock-free queue (high scalability, requires testing)
     * - ADAPTIVE: Start with legacy, switch based on runtime metrics (recommended)
     *
     * ### Example
     * @code
     * connection_pool_config config;
     * config.min_connections = 5;
     * config.max_connections = 50;
     *
     * auto factory = [&]() {
     *     return std::make_unique<postgres_database>(config.connection_string);
     * };
     *
     * // Create pool with adaptive queue (recommended)
     * connection_pool_v3 pool(
     *     database_types::postgresql,
     *     config,
     *     factory,
     *     std::thread::hardware_concurrency(),
     *     kcenon::thread::adaptive_typed_job_queue_t<connection_priority>::queue_strategy::ADAPTIVE
     * );
     *
     * if (!pool.initialize()) {
     *     std::cerr << "Failed to initialize pool\n";
     * }
     * @endcode
     */
    connection_pool_v3(
        database_types db_type,
        const connection_pool_config& config,
        std::function<std::unique_ptr<database_base>()> factory,
        size_t thread_count = std::thread::hardware_concurrency(),
        kcenon::thread::adaptive_typed_job_queue_t<connection_priority>::queue_strategy queue_strategy =
            kcenon::thread::adaptive_typed_job_queue_t<connection_priority>::queue_strategy::FORCE_LEGACY);

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~connection_pool_v3();

    // Prevent copying and moving
    connection_pool_v3(const connection_pool_v3&) = delete;
    connection_pool_v3& operator=(const connection_pool_v3&) = delete;
    connection_pool_v3(connection_pool_v3&&) = delete;
    connection_pool_v3& operator=(connection_pool_v3&&) = delete;

    /**
     * @brief Initializes the connection pool
     * @return true if initialization successful, false otherwise
     *
     * Must be called before acquiring connections.
     */
    bool initialize();

    /**
     * @brief Acquires a connection with specified priority
     * @param priority Priority level for this request (default: NORMAL_QUERY)
     * @return Future that resolves to Result<connection_wrapper>
     *
     * ### Thread Safety
     * Thread-safe, can be called from multiple threads concurrently.
     *
     * ### Performance
     * - Adaptive queue: Auto-selects best implementation (mutex or lock-free)
     * - Target latency: < 100ns scheduling + pool acquisition time
     * - Throughput: > 1M ops/s under high load
     *
     * ### Example
     * @code
     * // Critical operation (highest priority)
     * auto future = pool.acquire_connection(connection_priority::CRITICAL);
     * auto result = future.get();
     * if (result.is_ok()) {
     *     auto conn = result.value();
     *     conn->get()->execute_query("UPDATE accounts SET ...");
     * }
     * @endcode
     */
    std::future<Result<std::shared_ptr<connection_wrapper>>>
    acquire_connection(connection_priority priority = connection_priority::NORMAL_QUERY);

    /**
     * @brief Returns a connection to the pool
     * @param connection Connection to return
     *
     * Always return connections after use to avoid resource leaks.
     */
    void release_connection(std::shared_ptr<connection_wrapper> connection);

    /**
     * @brief Schedules asynchronous health check
     *
     * Health checks run as low-priority background jobs.
     */
    void schedule_health_check();

    /**
     * @brief Gets the number of active connections
     * @return Number of active connections
     */
    size_t active_connections() const;

    /**
     * @brief Gets the number of available connections
     * @return Number of available connections
     */
    size_t available_connections() const;

    /**
     * @brief Gets connection pool statistics
     * @return Connection statistics
     */
    connection_stats get_stats() const;

    /**
     * @brief Requests graceful shutdown via cancellation token
     *
     * Signals all pending operations to cancel cooperatively.
     * Does not block; call shutdown() to wait for completion.
     *
     * ### Example
     * @code
     * pool.request_shutdown();  // Signal cancellation
     * pool.shutdown();          // Wait for completion
     * @endcode
     */
    void request_shutdown();

    /**
     * @brief Shuts down the connection pool
     *
     * Waits for pending operations to complete before shutting down.
     */
    void shutdown();

    /**
     * @brief Checks if shutdown was requested
     * @return true if shutdown requested, false otherwise
     */
    [[nodiscard]] bool is_shutdown_requested() const;

    /**
     * @brief Gets the current adaptive queue type in use
     * @return String describing current queue implementation ("LEGACY_MUTEX", "LOCKFREE", etc.)
     *
     * ### Example
     * @code
     * std::cout << "Current queue: " << pool.get_current_queue_type() << "\n";
     * @endcode
     */
    [[nodiscard]] std::string get_current_queue_type() const;

    /**
     * @brief Gets the number of queue strategy switches
     * @return Number of times the adaptive queue has switched implementation
     */
    [[nodiscard]] uint64_t get_queue_switch_count() const;

    /**
     * @brief Gets the contention ratio
     * @return Percentage of operations that experienced contention (0.0-100.0)
     */
    [[nodiscard]] double get_contention_ratio() const;

    /**
     * @brief Gets the average queue operation latency
     * @return Average latency in nanoseconds
     */
    [[nodiscard]] double get_average_queue_latency_ns() const;

    /**
     * @brief Gets performance metrics for this pool
     * @return Shared pointer to priority-aware metrics
     *
     * ### Example
     * @code
     * auto metrics = pool.get_metrics();
     * std::cout << "Success rate: " << metrics->success_rate() << "%\n";
     * std::cout << "Avg latency: " << metrics->average_wait_time_us() << " μs\n";
     * std::cout << "CRITICAL avg: "
     *           << metrics->average_wait_time_for_priority(
     *                  connection_priority::CRITICAL) << " μs\n";
     * @endcode
     */
    std::shared_ptr<monitoring::priority_metrics<connection_priority>> get_metrics() const;

private:
    // Underlying connection pool (actual connection management)
    std::shared_ptr<connection_pool> underlying_pool_;

    // Adaptive job queue for priority-based scheduling
    std::shared_ptr<kcenon::thread::adaptive_typed_job_queue_t<connection_priority>> adaptive_queue_;

    // Worker thread pool
    std::shared_ptr<kcenon::thread::typed_thread_pool_t<connection_priority>> worker_pool_;

    // Cancellation token for graceful shutdown
    kcenon::thread::cancellation_token shutdown_token_;

    // Performance metrics with priority tracking
    std::shared_ptr<monitoring::priority_metrics<connection_priority>> metrics_;

    // Thread count
    size_t thread_count_;

    // Shutdown flag
    std::atomic<bool> shutdown_requested_;
};

} // namespace database::pooling
