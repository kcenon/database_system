// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file unified_database_system.cpp
 * @brief Implementation of unified database system (Phase 6)
 */

#include "unified_database_system.h"
#include "core/database_coordinator.h"
#include "adapters/logger_adapter.h"
#include "adapters/monitoring_adapter.h"
#include "adapters/thread_adapter.h"
#include "../core/database_backend.h"
#include "../core/backend_registry.h"
#include "../postgres_manager.h"

#include <mutex>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace database::integrated {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Helper to create error VoidResult
inline kcenon::common::VoidResult make_error(const std::string& msg, int code = -1, const std::string& context = "")
{
    return kcenon::common::VoidResult(kcenon::common::error_info{code, msg, context});
}

// Helper to create error Result<T>
template <typename T>
inline kcenon::common::Result<T> make_error_result(const std::string& msg, int code = -1, const std::string& context = "")
{
    return kcenon::common::Result<T>(kcenon::common::error_info{code, msg, context});
}

} // anonymous namespace


/**
 * @brief Convert backend_type to registry name string
 */
static std::string backend_type_to_name(backend_type type) {
    switch (type) {
        case backend_type::postgres: return "postgresql";
        case backend_type::sqlite:   return "sqlite";
        case backend_type::mongodb:  return "mongodb";
        case backend_type::redis:    return "redis";
        default:                     return "";
    }
}

/**
 * @brief Create database backend instance
 *
 * First tries hardcoded backends, then falls back to backend_registry
 * for dynamically registered backends (including test stubs).
 */
static std::shared_ptr<core::database_backend> create_backend(backend_type type) {
    switch (type) {
        case backend_type::postgres:
            return std::make_shared<postgres_manager>();
        default: {
            // Fall back to backend_registry for other types
            auto name = backend_type_to_name(type);
            if (!name.empty()) {
                auto backend = core::backend_registry::instance().create(name);
                if (backend) {
                    return std::shared_ptr<core::database_backend>(std::move(backend));
                }
            }
            return nullptr;
        }
    }
}

/**
 * @brief Convert database_value (variant) to string
 */
static std::string value_to_string(const core::database_value& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "NULL";
        }
        return "";
    }, value);
}

/**
 * @brief Convert core::database_result to query_result
 */
static query_result convert_result(
    const core::database_result& db_result,
    std::chrono::microseconds exec_time) {

    query_result result;
    result.rows.reserve(db_result.size());

    // Convert each row: map<string, database_value> -> map<string, string>
    for (const auto& db_row : db_result) {
        row_data converted_row;
        for (const auto& [key, value] : db_row) {
            converted_row[key] = value_to_string(value);
        }
        result.rows.push_back(std::move(converted_row));
    }

    result.affected_rows = db_result.size();
    result.execution_time = exec_time;

    return result;
}

// ============================================================================
// Transaction Implementation
// ============================================================================

class transaction_impl : public transaction {
public:
    explicit transaction_impl(std::shared_ptr<core::database_backend> backend)
        : backend_(std::move(backend)), active_(false) {

        // Begin transaction
        if (backend_) {
            auto result = backend_->begin_transaction();
            if (result.is_ok()) {
                active_ = true;
            }
        }
    }

    ~transaction_impl() override {
        if (active_) {
            // Auto-rollback if not committed
            backend_->rollback_transaction();
        }
    }

    kcenon::common::Result<query_result> execute(
        const std::string& query,
        const std::vector<query_param>& params) override {

        if (!active_) {
            return make_error_result<query_result>("Transaction not active", -1, "transaction");
        }

        // Note: Parameterized queries are not yet supported.
        // For now, params are ignored and query is executed as-is (ensure query is pre-sanitized).
        auto start = std::chrono::steady_clock::now();
        auto db_result = backend_->select_query(query);
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);

        if (db_result.is_err()) {
            return db_result.error();
        }

        return convert_result(db_result.value(), duration);
    }

    kcenon::common::VoidResult commit() override {
        if (!active_) {
            return make_error("Transaction not active", -1, "transaction");
        }

        auto result = backend_->commit_transaction();
        if (result.is_err()) {
            return make_error("Commit failed: " + result.error().message, -1, "transaction");
        }

        active_ = false;
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult rollback() override {
        if (!active_) {
            return make_error("Transaction not active", -1, "transaction");
        }

        auto result = backend_->rollback_transaction();
        if (result.is_err()) {
            return make_error("Rollback failed: " + result.error().message, -1, "transaction");
        }

        active_ = false;
        return kcenon::common::ok();
    }

    bool is_active() const override {
        return active_;
    }

private:
    std::shared_ptr<core::database_backend> backend_;
    bool active_;
};

// ============================================================================
// PIMPL Implementation
// ============================================================================

class unified_database_system::impl {
public:
    impl(const unified_db_config& config)
        : config_(config)
        , coordinator_(std::make_unique<database_coordinator>(config))
        , backend_type_(backend_type::postgres)
        , connected_(false)
    {
        // Initialize coordinator
        auto init_result = coordinator_->initialize();
        if (!init_result.is_ok()) {
            throw std::runtime_error("Failed to initialize database coordinator: " +
                init_result.error().message);
        }

        // Initialize metrics
        metrics_.measurement_start = std::chrono::steady_clock::now();
    }

    ~impl() {
        disconnect();
        coordinator_->shutdown();
    }

    // Connection management

    kcenon::common::VoidResult connect(backend_type backend, const std::string& connection_string) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connected_) {
            return make_error("Already connected", -1, "unified_database_system");
        }

        backend_type_ = backend;
        connection_string_ = connection_string;

        // Create backend instance
        backend_ = create_backend(backend);
        if (!backend_) {
            return make_error("Unsupported backend type", -2, "unified_database_system");
        }

        // Connect to database using connection_config
        auto config = core::connection_config::from_string(connection_string);
        auto result = backend_->initialize(config);
        if (result.is_err()) {
            return make_error("Connection failed: " + result.error().message, -3, "unified_database_system");
        }

        connected_ = true;

        // Log connection
        if (auto* logger = coordinator_->get_logger()) {
            logger->log_connection_event(
                "connected",
                "Connection established to: " + connection_string
            );
        }

        return kcenon::common::ok();
    }

    kcenon::common::VoidResult disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return kcenon::common::ok();
        }

        // Disconnect backend
        if (backend_) {
            backend_->shutdown();
            backend_.reset();
        }

        connected_ = false;

        // Log disconnection
        if (auto* logger = coordinator_->get_logger()) {
            logger->log_connection_event(
                "disconnected",
                "Connection closed: " + connection_string_
            );
        }

        return kcenon::common::ok();
    }

    bool is_connected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_;
    }

    // Query execution (sync)

    kcenon::common::Result<query_result> execute(
        const std::string& query,
        const std::vector<query_param>& params) {

        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return make_error_result<query_result>("Not connected to database", -1, "unified_database_system");
        }

        // Record query start
        auto start = std::chrono::steady_clock::now();

        // Execute query
        auto db_result = backend_->select_query(query);

        // Calculate execution time
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);

        if (db_result.is_err()) {
            update_metrics(duration, false);
            return db_result.error();
        }

        // Update metrics
        update_metrics(duration, true);

        // Log query
        if (auto* logger = coordinator_->get_logger()) {
            logger->log(
                config_.logger.min_log_level,
                "Query executed: " + query + " (" + std::to_string(duration.count()) + "us)"
            );
        }

        // Record metrics
        if (auto* monitor = coordinator_->get_monitor()) {
            monitor->record_query_execution(duration, true);
        }

        return convert_result(db_result.value(), duration);
    }

    // Query execution (async)

    std::future<kcenon::common::Result<query_result>> execute_async(
        const std::string& query,
        const std::vector<query_param>& params) {

        // Submit to thread pool
        auto* thread_pool = coordinator_->get_thread_pool();
        if (!thread_pool) {
            std::promise<kcenon::common::Result<query_result>> promise;
            promise.set_value(make_error_result<query_result>("Thread pool not available", -1, "unified_database_system"));
            return promise.get_future();
        }

        // Create task
        return thread_pool->submit([this, query, params]() -> kcenon::common::Result<query_result> {
            return this->execute(query, params);
        });
    }

    std::future<kcenon::common::Result<query_result>> execute_async_priority(
        const std::string& query,
        int priority,
        const std::vector<query_param>& params) {

        auto* thread_pool = coordinator_->get_thread_pool();
        if (!thread_pool) {
            std::promise<kcenon::common::Result<query_result>> promise;
            promise.set_value(make_error_result<query_result>("Thread pool not available", -1, "unified_database_system"));
            return promise.get_future();
        }

        // Note: Priority parameter is currently ignored
        // Backend pattern removed priority support for simplification
        (void)priority; // Suppress unused parameter warning

        return thread_pool->submit([this, query, params]() -> kcenon::common::Result<query_result> {
            return this->execute(query, params);
        });
    }

    // Transaction management

    kcenon::common::Result<std::unique_ptr<transaction>> begin_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return make_error_result<std::unique_ptr<transaction>>("Not connected to database", -1, "unified_database_system");
        }

        ++metrics_.transactions_started;

        auto tx_impl = std::make_unique<transaction_impl>(backend_);
        std::unique_ptr<transaction> tx = std::move(tx_impl);
        return tx;
    }

    kcenon::common::VoidResult execute_transaction(const std::vector<std::string>& queries) {
        auto tx_result = begin_transaction();
        if (!tx_result.is_ok()) {
            return tx_result.error();
        }

        auto tx = std::move(tx_result.value());

        for (const auto& query : queries) {
            auto result = tx->execute(query, {});
            if (!result.is_ok()) {
                tx->rollback();
                ++metrics_.transactions_rolled_back;
                return result.error();
            }
        }

        auto commit_result = tx->commit();
        if (commit_result.is_ok()) {
            ++metrics_.transactions_committed;
        } else {
            ++metrics_.transactions_rolled_back;
        }

        return commit_result;
    }

    // Monitoring

    database_metrics get_metrics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return metrics_;
    }

    health_check check_health() const {
        std::lock_guard<std::mutex> lock(mutex_);

        health_check health;
        health.last_check = std::chrono::steady_clock::now();
        health.is_connected = connected_;

        // Check coordinator health
        auto coord_health = coordinator_->check_health();
        if (coord_health.is_ok() && coord_health.value()) {
            health.logger_healthy = true;
            health.monitor_healthy = true;
            health.thread_pool_healthy = true;
        }

        // Connection pooling not yet implemented on client side
        health.connection_pool_healthy = true;
        health.connection_pool_utilization = 0.0;

        // Determine overall health
        if (!connected_) {
            health.status = health_status::failed;
            health.issues.push_back("Not connected to database");
        } else {
            health.status = health_status::healthy;
        }

        return health;
    }

    void reset_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_ = database_metrics{};
        metrics_.measurement_start = std::chrono::steady_clock::now();
    }

    // Configuration access

    const unified_db_config& get_config() const {
        return config_;
    }

    backend_type get_backend_type() const {
        return backend_type_;
    }

    unified_database_system::pool_stats get_pool_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);

        // Connection pooling not yet implemented on client side
        // Return empty stats for client-side
        unified_database_system::pool_stats stats;
        stats.total_connections = connected_ ? 1 : 0;
        stats.active_connections = connected_ ? 1 : 0;
        stats.idle_connections = 0;
        stats.wait_queue_size = 0;
        stats.utilization_percent = connected_ ? 100.0 : 0.0;

        return stats;
    }

    // Query builder

    query_builder create_query_builder() const {
        return query_builder{database_types::sqlite};
    }

private:
    void update_metrics(std::chrono::microseconds latency, bool success) {
        ++metrics_.total_queries;

        if (success) {
            ++metrics_.successful_queries;
        } else {
            ++metrics_.failed_queries;
        }

        // Update latency stats
        if (metrics_.min_latency == std::chrono::microseconds{0} ||
            latency < metrics_.min_latency) {
            metrics_.min_latency = latency;
        }

        if (latency > metrics_.max_latency) {
            metrics_.max_latency = latency;
        }

        // Exponential Moving Average (EMA) with alpha = 0.1 (gives more weight to recent values)
        // EMA formula: EMA_new = alpha * value + (1 - alpha) * EMA_old
        constexpr double alpha = 0.1;
        if (metrics_.total_queries == 1) {
            // First query: initialize with actual latency
            metrics_.average_latency = latency;
        } else {
            // Subsequent queries: apply EMA formula
            auto current_avg_us = metrics_.average_latency.count();
            auto new_latency_us = latency.count();
            auto ema_us = static_cast<int64_t>(alpha * new_latency_us + (1.0 - alpha) * current_avg_us);
            metrics_.average_latency = std::chrono::microseconds{ema_us};
        }

        // Calculate throughput
        auto duration = std::chrono::steady_clock::now() - metrics_.measurement_start;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        if (seconds > 0) {
            metrics_.queries_per_second = static_cast<double>(metrics_.total_queries) / seconds;
        }

        // Check for slow query
        if (latency > config_.logger.slow_query_threshold) {
            ++metrics_.slow_queries;
        }
    }

    unified_db_config config_;
    std::unique_ptr<database_coordinator> coordinator_;

    backend_type backend_type_;
    std::string connection_string_;
    bool connected_;

    std::shared_ptr<core::database_backend> backend_; // shared_ptr for transaction support

    database_metrics metrics_;

    mutable std::mutex mutex_;
};

// ============================================================================
// unified_database_system Implementation
// ============================================================================

// Constructors

unified_database_system::unified_database_system()
    : unified_database_system(unified_db_config{}) {
}

unified_database_system::unified_database_system(const unified_db_config& config)
    : pimpl_(std::make_unique<impl>(config)) {
}

unified_database_system::~unified_database_system() = default;

unified_database_system::unified_database_system(unified_database_system&&) noexcept = default;
unified_database_system& unified_database_system::operator=(unified_database_system&&) noexcept = default;

// Connection management

kcenon::common::VoidResult unified_database_system::connect(const std::string& connection_string) {
    return pimpl_->connect(backend_type::postgres, connection_string);
}

kcenon::common::VoidResult unified_database_system::connect(
    backend_type backend,
    const std::string& connection_string) {
    return pimpl_->connect(backend, connection_string);
}

kcenon::common::VoidResult unified_database_system::disconnect() {
    return pimpl_->disconnect();
}

bool unified_database_system::is_connected() const {
    return pimpl_->is_connected();
}

// Query execution (sync)

kcenon::common::Result<query_result> unified_database_system::execute(
    const std::string& query,
    const std::vector<query_param>& params) {
    return pimpl_->execute(query, params);
}

kcenon::common::Result<query_result> unified_database_system::select(
    const std::string& query,
    const std::vector<query_param>& params) {
    return pimpl_->execute(query, params);
}

kcenon::common::Result<size_t> unified_database_system::insert(
    const std::string& query,
    const std::vector<query_param>& params) {
    auto result = pimpl_->execute(query, params);
    if (result.is_ok()) {
        return result.value().affected_rows;
    }
    return result.error();
}

kcenon::common::Result<size_t> unified_database_system::update(
    const std::string& query,
    const std::vector<query_param>& params) {
    auto result = pimpl_->execute(query, params);
    if (result.is_ok()) {
        return result.value().affected_rows;
    }
    return result.error();
}

kcenon::common::Result<size_t> unified_database_system::remove(
    const std::string& query,
    const std::vector<query_param>& params) {
    auto result = pimpl_->execute(query, params);
    if (result.is_ok()) {
        return result.value().affected_rows;
    }
    return result.error();
}

// Query execution (async)

std::future<kcenon::common::Result<query_result>> unified_database_system::execute_async(
    const std::string& query,
    const std::vector<query_param>& params) {
    return pimpl_->execute_async(query, params);
}

std::future<kcenon::common::Result<query_result>> unified_database_system::execute_async_priority(
    const std::string& query,
    int priority,
    const std::vector<query_param>& params) {
    return pimpl_->execute_async_priority(query, priority, params);
}

// Transaction management

kcenon::common::Result<std::unique_ptr<transaction>> unified_database_system::begin_transaction() {
    return pimpl_->begin_transaction();
}

kcenon::common::VoidResult unified_database_system::execute_transaction(
    const std::vector<std::string>& queries) {
    return pimpl_->execute_transaction(queries);
}

// Monitoring

database_metrics unified_database_system::get_metrics() const {
    return pimpl_->get_metrics();
}

health_check unified_database_system::check_health() const {
    return pimpl_->check_health();
}

void unified_database_system::reset_metrics() {
    pimpl_->reset_metrics();
}

// Configuration access

const unified_db_config& unified_database_system::get_config() const {
    return pimpl_->get_config();
}

backend_type unified_database_system::get_backend_type() const {
    return pimpl_->get_backend_type();
}

unified_database_system::pool_stats unified_database_system::get_pool_stats() const {
    return pimpl_->get_pool_stats();
}

// Query builder

query_builder unified_database_system::create_query_builder() const {
    return pimpl_->create_query_builder();
}

// ============================================================================
// Builder Implementation
// ============================================================================

unified_database_system::builder::builder() {
    // Set smart defaults
    config_.connection_pool.min_connections = 2;
    config_.connection_pool.max_connections = 10;
    config_.connection_pool.connection_timeout = std::chrono::seconds(5);

    config_.logger.min_log_level = db_log_level::info;
    config_.logger.enable_file_logging = true;
    config_.logger.log_directory = "./logs";
    config_.logger.slow_query_threshold = std::chrono::milliseconds(1000);

    config_.monitoring.enable_metrics = true;
    config_.monitoring.metrics_interval = std::chrono::seconds(60);

    config_.thread.thread_count = 4;
}

unified_database_system::builder& unified_database_system::builder::set_backend(
    backend_type type) {
    config_.database.type = type;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::set_connection_string(
    const std::string& conn_str) {
    connection_string_ = conn_str;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::set_pool_size(
    size_t min_size,
    size_t max_size) {
    config_.connection_pool.min_connections = min_size;
    config_.connection_pool.max_connections = max_size;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::enable_logging(
    db_log_level level,
    const std::string& log_dir) {
    config_.logger.min_log_level = level;
    config_.logger.log_directory = log_dir;
    config_.logger.enable_file_logging = true;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::enable_monitoring(
    bool enable) {
    config_.monitoring.enable_metrics = enable;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::enable_async(
    size_t worker_threads) {
    config_.thread.thread_count = worker_threads;
    return *this;
}

unified_database_system::builder& unified_database_system::builder::set_slow_query_threshold(
    std::chrono::milliseconds threshold) {
    config_.logger.slow_query_threshold = threshold;
    return *this;
}

std::unique_ptr<unified_database_system> unified_database_system::builder::build() {
    auto system = std::make_unique<unified_database_system>(config_);

    // Auto-connect if connection string provided
    if (!connection_string_.empty()) {
        auto result = system->connect(config_.database.type, connection_string_);
        if (!result.is_ok()) {
            throw std::runtime_error("Failed to connect: " + result.error().message);
        }
    }

    return system;
}

unified_database_system::builder unified_database_system::create_builder() {
    return builder{};
}

} // namespace database::integrated
