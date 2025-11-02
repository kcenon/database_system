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

#include "../adapters/thread_pool_adapter.h"
#include "../connection_pool.h"
#include "../database_base.h"
#include "../database_types.h"
#include <memory>
#include <chrono>
#include <future>
#include <functional>

#ifdef USE_THREAD_SYSTEM
    #include <kcenon/thread/core/typed_thread_pool.h>
    #include <kcenon/thread/core/typed_thread_worker.h>
    #include <kcenon/thread/core/error_handling.h>
#endif

namespace database::pooling {

/**
 * @enum connection_priority
 * @brief Priority levels for connection acquisition requests
 *
 * These priority levels determine the order in which connection requests
 * are serviced by the connection_pool_v2. Higher priority requests are
 * processed first when multiple requests are pending.
 *
 * ### Priority Order (Highest to Lowest)
 * 1. CRITICAL - Time-sensitive operations that cannot be delayed
 * 2. TRANSACTION - Active transactions requiring immediate response
 * 3. NORMAL_QUERY - Standard database queries (default priority)
 * 4. HEALTH_CHECK - Background health monitoring (lowest priority)
 *
 * ### Usage Example
 * @code
 * // Critical operation (e.g., payment processing)
 * auto result = pool.acquire_connection(connection_priority::CRITICAL);
 *
 * // Normal query (default)
 * auto result = pool.acquire_connection(connection_priority::NORMAL_QUERY);
 *
 * // Background health check
 * auto result = pool.acquire_connection(connection_priority::HEALTH_CHECK);
 * @endcode
 */
enum class connection_priority : int {
    HEALTH_CHECK  = 0,  ///< Lowest priority - background maintenance
    NORMAL_QUERY  = 1,  ///< Default priority - standard queries
    TRANSACTION   = 2,  ///< High priority - active transactions
    CRITICAL      = 3   ///< Highest priority - time-critical operations
};

#ifdef USE_THREAD_SYSTEM
// Forward declarations from thread_system
template<typename JobType>
class connection_request_job;

template<typename JobType>
class health_check_job;

/**
 * @class connection_request_job
 * @brief Typed job for priority-based connection acquisition
 *
 * This job wraps connection acquisition logic and integrates with
 * thread_system's typed_thread_pool for priority-based scheduling.
 */
template<>
class connection_request_job<connection_priority> : public kcenon::thread::typed_job_t<connection_priority> {
public:
    using completion_callback = std::function<void(Result<std::shared_ptr<connection_wrapper>>)>;

    /**
     * @brief Constructs a connection request job
     * @param priority Priority level for this request
     * @param pool_ref Reference to the legacy connection pool
     * @param callback Callback to invoke with the result
     */
    explicit connection_request_job(
        connection_priority priority,
        std::shared_ptr<connection_pool_base> pool_ref,
        completion_callback callback)
        : kcenon::thread::typed_job_t<connection_priority>(priority, "connection_request")
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
                callback_(error_info{-599, std::string("Exception in connection request: ") + e.what(), "connection_pool_v2"});
            }

            return kcenon::thread::error{
                kcenon::thread::error_code::job_execution_failed,
                std::string("Exception in connection_request_job: ") + e.what()
            };
        }
    }

private:
    std::shared_ptr<connection_pool_base> pool_ref_;
    completion_callback callback_;
};

/**
 * @class health_check_job
 * @brief Low-priority job for asynchronous health checks
 */
template<>
class health_check_job<connection_priority> : public kcenon::thread::typed_job_t<connection_priority> {
public:
    explicit health_check_job(std::shared_ptr<connection_pool> pool_ref)
        : kcenon::thread::typed_job_t<connection_priority>(connection_priority::HEALTH_CHECK, "health_check")
        , pool_ref_(std::move(pool_ref))
    {}

    kcenon::thread::result_void do_work() override {
        try {
            pool_ref_->health_check();
            return kcenon::thread::result_void{};
        } catch (const std::exception& e) {
            return kcenon::thread::error{
                kcenon::thread::error_code::job_execution_failed,
                std::string("Exception in health_check_job: ") + e.what()
            };
        }
    }

private:
    std::shared_ptr<connection_pool> pool_ref_;
};
#endif // USE_THREAD_SYSTEM

/**
 * @class connection_pool_v2
 * @brief High-performance connection pool with priority-based scheduling
 *
 * This is a next-generation connection pool that leverages thread_system's
 * typed_thread_pool for priority-based connection request handling:
 * - **Priority Scheduling**: Critical operations get connections first
 * - **Async Health Checks**: Non-blocking background maintenance
 * - **High Throughput**: Leverages thread_system's 1.16M+ jobs/s capacity
 * - **Low Latency**: 77ns job scheduling overhead
 *
 * ### Architecture
 * - Wraps existing connection_pool for actual connection management
 * - Uses typed_thread_pool<connection_priority> for request scheduling
 * - Health checks run as low-priority background jobs
 *
 * ### Priority Levels
 * - CRITICAL: Payment processing, real-time data updates
 * - TRANSACTION: Active transaction queries
 * - NORMAL_QUERY: Standard SELECT/INSERT/UPDATE queries (default)
 * - HEALTH_CHECK: Background connection validation
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called from multiple threads.
 *
 * ### Performance Comparison
 * - **connection_pool_v2**: Priority-aware, 77ns scheduling latency
 * - **connection_pool**: FIFO order, mutex-based synchronization
 *
 * ### Migration Path
 * @code
 * // Old code (still works)
 * connection_pool pool(db_type, config, factory);
 * auto result = pool.acquire_connection();
 *
 * // New code (with priorities)
 * connection_pool_v2 pool_v2(db_type, config, factory);
 * auto result = pool_v2.acquire_connection(connection_priority::CRITICAL);
 * @endcode
 */
class connection_pool_v2 {
public:
    /**
     * @brief Constructs a connection pool with priority scheduling
     * @param db_type Database type for this pool
     * @param config Pool configuration
     * @param factory Function to create new database connections
     * @param thread_count Number of worker threads for the scheduler (default: hardware_concurrency)
     *
     * ### Example
     * @code
     * connection_pool_config config;
     * config.min_connections = 5;
     * config.max_connections = 20;
     *
     * auto factory = [&config]() {
     *     return std::make_unique<postgres_database>(config.connection_string);
     * };
     *
     * connection_pool_v2 pool(database_types::postgresql, config, factory);
     * if (!pool.initialize()) {
     *     std::cerr << "Failed to initialize pool\n";
     * }
     * @endcode
     */
    connection_pool_v2(
        database_types db_type,
        const connection_pool_config& config,
        std::function<std::unique_ptr<database_base>()> factory,
        size_t thread_count = std::thread::hardware_concurrency());

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~connection_pool_v2();

    // Prevent copying and moving
    connection_pool_v2(const connection_pool_v2&) = delete;
    connection_pool_v2& operator=(const connection_pool_v2&) = delete;
    connection_pool_v2(connection_pool_v2&&) = delete;
    connection_pool_v2& operator=(connection_pool_v2&&) = delete;

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
     * - thread_system: 77ns scheduling + pool acquisition time
     * - Fallback: Direct synchronous acquisition
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
     *
     * // Normal query (default priority)
     * auto future = pool.acquire_connection();
     * auto result = future.get();
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
     * Health checks run as low-priority background jobs, ensuring they
     * don't interfere with critical operations.
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
     * @brief Shuts down the connection pool
     *
     * Waits for pending operations to complete before shutting down.
     */
    void shutdown();

    /**
     * @brief Checks if using thread_system implementation
     * @return true if using thread_system, false if using fallback
     */
    constexpr bool is_using_thread_system() const {
        return async::using_thread_system;
    }

private:
    // Underlying connection pool (actual connection management)
    std::shared_ptr<connection_pool> underlying_pool_;

#ifdef USE_THREAD_SYSTEM
    // Priority-based job scheduler
    std::shared_ptr<kcenon::thread::typed_thread_pool_t<connection_priority>> scheduler_pool_;
    size_t thread_count_;
#endif

    std::atomic<bool> shutdown_requested_;
};

} // namespace database::pooling
