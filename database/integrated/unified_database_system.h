// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file unified_database_system.h
 * @brief Zero-configuration database system with integrated adapters (Phase 6)
 *
 * This is the main entry point for the unified database system. It provides
 * a simple, modern API that integrates all adapters (logger, monitoring, thread)
 * behind the scenes using database_coordinator.
 *
 * **Key Features:**
 * - Zero-configuration with smart defaults
 * - Builder pattern for custom configuration
 * - Synchronous and asynchronous query execution
 * - Transaction management
 * - Connection pooling
 * - Integrated logging, monitoring, and async operations
 * - Backward compatible with legacy database_manager
 *
 * **Example Usage:**
 *
 * @code
 * using namespace database::integrated;
 *
 * // 1. Zero-config usage (simplest)
 * unified_database_system db;
 * auto result = db.connect("postgresql://localhost/mydb");
 * if (result.is_ok()) {
 *     auto rows = db.execute("SELECT * FROM users WHERE id = $1", 42);
 * }
 *
 * // 2. Builder pattern configuration
 * auto db = unified_database_system::builder()
 *     .set_backend(backend_type::postgresql)
 *     .set_connection_string("host=localhost dbname=mydb")
 *     .set_pool_size(10, 50)
 *     .enable_logging(db_log_level::debug, "./logs")
 *     .enable_monitoring(true)
 *     .enable_async(4)  // 4 worker threads
 *     .build();
 *
 * // 3. Async query execution
 * auto future = db->execute_async("SELECT * FROM large_table");
 * // Do other work...
 * auto result = future.get();
 *
 * // 4. Transaction management
 * auto tx = db->begin_transaction();
 * tx->execute("INSERT INTO users (name) VALUES ($1)", "Alice");
 * tx->execute("UPDATE accounts SET balance = balance - 100");
 * tx->commit();
 *
 * // 5. Query builder integration
 * auto query = db->query_builder()
 *     .select("users")
 *     .where("age > ?", 18)
 *     .order_by("name")
 *     .limit(10)
 *     .build();
 * auto users = db->execute(query);
 *
 * // 6. Health check and metrics
 * auto health = db->check_health();
 * auto metrics = db->get_metrics();
 * std::cout << "Query throughput: " << metrics.queries_per_second << "\n";
 * @endcode
 */

#pragma once

#include "core/configuration.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <future>
#include <chrono>
#include <functional>
#include <optional>

// Use unified Result<T> implementation
#include "../core/result.h"

// Include query builder
#include "../query_builder.h"

namespace database::integrated {

// Forward declarations
class database_coordinator;
class transaction;

/**
 * @brief Database row representation
 */
using row_data = std::map<std::string, std::string>;

/**
 * @brief Query result set
 */
struct query_result {
    std::vector<row_data> rows;
    size_t affected_rows{0};
    std::chrono::microseconds execution_time{0};

    bool empty() const { return rows.empty(); }
    size_t size() const { return rows.size(); }

    // Convenience accessors
    const row_data& operator[](size_t index) const { return rows.at(index); }
    row_data& operator[](size_t index) { return rows.at(index); }

    auto begin() { return rows.begin(); }
    auto end() { return rows.end(); }
    auto begin() const { return rows.begin(); }
    auto end() const { return rows.end(); }
};

/**
 * @brief Database performance metrics
 */
struct database_metrics {
    // Query statistics
    size_t total_queries{0};
    size_t successful_queries{0};
    size_t failed_queries{0};
    size_t slow_queries{0};

    // Latency statistics
    std::chrono::microseconds average_latency{0};
    std::chrono::microseconds min_latency{0};
    std::chrono::microseconds max_latency{0};
    std::chrono::microseconds p95_latency{0};
    std::chrono::microseconds p99_latency{0};

    // Connection pool statistics
    size_t pool_size{0};
    size_t active_connections{0};
    size_t idle_connections{0};
    size_t wait_queue_size{0};

    // Throughput
    double queries_per_second{0.0};
    std::chrono::steady_clock::time_point measurement_start;

    // Transaction statistics
    size_t transactions_started{0};
    size_t transactions_committed{0};
    size_t transactions_rolled_back{0};
};

/**
 * @brief Database health status
 */
enum class health_status {
    healthy,        // All systems operational
    degraded,       // Some issues, but operational
    critical,       // Major issues, limited functionality
    failed          // System not operational
};

/**
 * @brief Health check result
 */
struct health_check {
    health_status status{health_status::healthy};
    bool is_connected{false};
    bool logger_healthy{false};
    bool monitor_healthy{false};
    bool thread_pool_healthy{false};
    bool connection_pool_healthy{false};

    double connection_pool_utilization{0.0};
    std::vector<std::string> issues;
    std::chrono::steady_clock::time_point last_check;
};

/**
 * @brief Query parameter value with null safety
 *
 * Supports implicit conversions from common types while providing null safety.
 * When a null pointer is passed, is_null() returns true and value() returns empty string.
 *
 * @example
 * // Normal usage
 * db.execute("SELECT * FROM users WHERE id = $1", {42});
 * db.execute("SELECT * FROM users WHERE name = $1", {"Alice"});
 *
 * // Null handling
 * const char* ptr = get_nullable_string();  // might return nullptr
 * db.execute("SELECT * FROM users WHERE name = $1", {ptr});  // safe, won't crash
 *
 * // Explicit null
 * db.execute("UPDATE users SET deleted_at = $1", {nullptr});
 */
struct query_param {
    std::optional<std::string> value;

    // Implicit conversions for convenience with null safety
    query_param(const std::string& v) : value(v) {}
    query_param(std::string&& v) : value(std::move(v)) {}
    query_param(const char* v) : value(v ? std::optional<std::string>(v) : std::nullopt) {}
    query_param(std::nullptr_t) : value(std::nullopt) {}
    query_param(int v) : value(std::to_string(v)) {}
    query_param(long v) : value(std::to_string(v)) {}
    query_param(long long v) : value(std::to_string(v)) {}
    query_param(unsigned int v) : value(std::to_string(v)) {}
    query_param(unsigned long v) : value(std::to_string(v)) {}
    query_param(unsigned long long v) : value(std::to_string(v)) {}
    query_param(double v) : value(std::to_string(v)) {}
    query_param(float v) : value(std::to_string(v)) {}
    query_param(bool v) : value(v ? "true" : "false") {}

    /**
     * @brief Check if this parameter represents a NULL value
     * @return true if the parameter is null
     */
    bool is_null() const noexcept { return !value.has_value(); }

    /**
     * @brief Get the string value (returns empty string for null)
     * @return The parameter value as string, or empty string if null
     * @note Use is_null() to distinguish between empty string and null
     */
    const std::string& get_value() const noexcept {
        static const std::string empty_string;
        return value.has_value() ? *value : empty_string;
    }

    /**
     * @brief Get the value for SQL generation
     * @return "NULL" for null values, otherwise the quoted/escaped value
     */
    std::string to_sql_string() const {
        return value.has_value() ? *value : "NULL";
    }
};

/**
 * @brief Transaction interface for ACID operations
 */
class transaction {
public:
    virtual ~transaction() = default;

    /**
     * @brief Execute a query within the transaction
     * @param query SQL query string
     * @param params Optional query parameters
     * @return Result containing query results or error
     */
    virtual kcenon::common::Result<query_result> execute(
        const std::string& query,
        const std::vector<query_param>& params = {}) = 0;

    /**
     * @brief Commit the transaction
     * @return Success or error result
     */
    virtual kcenon::common::VoidResult commit() = 0;

    /**
     * @brief Rollback the transaction
     * @return Success or error result
     */
    virtual kcenon::common::VoidResult rollback() = 0;

    /**
     * @brief Check if transaction is active
     * @return true if transaction is active
     */
    virtual bool is_active() const = 0;
};

/**
 * @brief Main unified database system class
 *
 * This class provides a simple, modern API for database operations
 * with integrated logging, monitoring, and async capabilities.
 *
 * **Thread Safety:** All methods are thread-safe
 * **Exception Safety:** No-throw guarantee (all errors returned via Result)
 * **Resource Safety:** RAII - resources cleaned up in destructor
 */
class unified_database_system {
public:
    /**
     * @brief Builder class for custom configuration
     */
    class builder {
    public:
        builder();

        /**
         * @brief Set the database backend type
         */
        builder& set_backend(backend_type type);

        /**
         * @brief Set the connection string
         */
        builder& set_connection_string(const std::string& conn_str);

        /**
         * @brief Set connection pool size
         * @param min_size Minimum number of connections
         * @param max_size Maximum number of connections
         */
        builder& set_pool_size(size_t min_size, size_t max_size);

        /**
         * @brief Enable logging
         * @param level Log level (debug, info, warning, error)
         * @param log_dir Directory for log files
         */
        builder& enable_logging(db_log_level level, const std::string& log_dir = "./logs");

        /**
         * @brief Enable monitoring and metrics collection
         * @param enable true to enable monitoring
         */
        builder& enable_monitoring(bool enable = true);

        /**
         * @brief Enable async operations
         * @param worker_threads Number of worker threads for async operations
         */
        builder& enable_async(size_t worker_threads = 4);

        /**
         * @brief Set slow query threshold
         * @param threshold Queries taking longer than this are logged as slow
         */
        builder& set_slow_query_threshold(std::chrono::milliseconds threshold);

        /**
         * @brief Build and return the configured database system
         * @return Result containing unique pointer to configured system, or error
         */
        kcenon::common::Result<std::unique_ptr<unified_database_system>> build();

    private:
        unified_db_config config_;
        std::string connection_string_;
    };

    /**
     * @brief Create a builder for custom configuration
     */
    static builder create_builder();

    // Constructors and destructor

    /**
     * @brief Default constructor (zero-config)
     *
     * Creates a database system with smart defaults:
     * - No backend specified (must call connect)
     * - Connection pool: min=2, max=10
     * - Logging: info level, console + file
     * - Monitoring: enabled
     * - Async: 4 worker threads
     */
    unified_database_system();

    /**
     * @brief Construct with configuration
     * @param config Database configuration
     */
    explicit unified_database_system(const unified_db_config& config);

    /**
     * @brief Destructor - automatically disconnects and cleans up
     */
    ~unified_database_system();

    // Disable copy, enable move
    unified_database_system(const unified_database_system&) = delete;
    unified_database_system& operator=(const unified_database_system&) = delete;
    unified_database_system(unified_database_system&&) noexcept;
    unified_database_system& operator=(unified_database_system&&) noexcept;

    // Connection management

    /**
     * @brief Connect to database
     * @param connection_string Database connection string (format depends on backend)
     * @return Success or error result
     *
     * Examples:
     * - PostgreSQL: "host=localhost port=5432 dbname=mydb user=user password=pass"
     * - SQLite: "mydb.db" or ":memory:"
     */
    kcenon::common::VoidResult connect(const std::string& connection_string);

    /**
     * @brief Connect to database with specific backend
     * @param backend Database backend type
     * @param connection_string Database connection string
     * @return Success or error result
     */
    kcenon::common::VoidResult connect(backend_type backend, const std::string& connection_string);

    /**
     * @brief Disconnect from database
     * @return Success or error result
     */
    kcenon::common::VoidResult disconnect();

    /**
     * @brief Check if connected to database
     * @return true if connected
     */
    bool is_connected() const;

    // Query execution (synchronous)

    /**
     * @brief Execute a SQL query synchronously
     * @param query SQL query string
     * @param params Optional query parameters
     * @return Result containing query results or error
     *
     * @example
     * auto result = db.execute("SELECT * FROM users WHERE id = $1", {42});
     */
    kcenon::common::Result<query_result> execute(
        const std::string& query,
        const std::vector<query_param>& params = {});

    /**
     * @brief Execute a SELECT query
     * @param query SQL SELECT statement
     * @param params Optional query parameters
     * @return Result containing selected rows or error
     *
     * @note This is a convenience wrapper around execute() that returns
     *       the full query_result. Use this method when you need all
     *       columns and rows from a SELECT statement.
     *
     * @see execute() for the underlying implementation
     */
    kcenon::common::Result<query_result> select(
        const std::string& query,
        const std::vector<query_param>& params = {});

    /**
     * @brief Execute an INSERT query
     * @param query SQL INSERT statement
     * @param params Optional query parameters
     * @return Result containing number of inserted rows or error
     *
     * @note This is a convenience wrapper around execute() that extracts
     *       only the affected_rows count from the query_result. Use this
     *       method when you only need to know how many rows were inserted.
     *
     * @see execute() for the underlying implementation
     */
    kcenon::common::Result<size_t> insert(
        const std::string& query,
        const std::vector<query_param>& params = {});

    /**
     * @brief Execute an UPDATE query
     * @param query SQL UPDATE statement
     * @param params Optional query parameters
     * @return Result containing number of updated rows or error
     *
     * @note This is a convenience wrapper around execute() that extracts
     *       only the affected_rows count from the query_result. Use this
     *       method when you only need to know how many rows were updated.
     *
     * @see execute() for the underlying implementation
     */
    kcenon::common::Result<size_t> update(
        const std::string& query,
        const std::vector<query_param>& params = {});

    /**
     * @brief Execute a DELETE query
     * @param query SQL DELETE statement
     * @param params Optional query parameters
     * @return Result containing number of deleted rows or error
     *
     * @note This is a convenience wrapper around execute() that extracts
     *       only the affected_rows count from the query_result. Use this
     *       method when you only need to know how many rows were deleted.
     *
     * @see execute() for the underlying implementation
     */
    kcenon::common::Result<size_t> remove(
        const std::string& query,
        const std::vector<query_param>& params = {});

    // Query execution (asynchronous)

    /**
     * @brief Execute a SQL query asynchronously
     * @param query SQL query string
     * @param params Optional query parameters
     * @return Future that will contain query results
     *
     * @example
     * auto future = db.execute_async("SELECT * FROM large_table");
     * // Do other work...
     * auto result = future.get();
     */
    std::future<kcenon::common::Result<query_result>> execute_async(
        const std::string& query,
        const std::vector<query_param>& params = {});

    /**
     * @brief Execute a SQL query asynchronously with priority
     * @param query SQL query string
     * @param priority Task priority (0=lowest, 100=highest)
     * @param params Optional query parameters
     * @return Future that will contain query results
     */
    std::future<kcenon::common::Result<query_result>> execute_async_priority(
        const std::string& query,
        int priority,
        const std::vector<query_param>& params = {});

    // Transaction management

    /**
     * @brief Begin a new transaction
     * @return Result containing transaction object or error
     *
     * @example
     * auto tx_result = db.begin_transaction();
     * if (tx_result.is_ok()) {
     *     auto tx = std::move(tx_result.value());
     *     tx->execute("INSERT INTO users (name) VALUES ($1)", {"Alice"});
     *     tx->commit();
     * }
     */
    kcenon::common::Result<std::unique_ptr<transaction>> begin_transaction();

    /**
     * @brief Execute multiple queries in a transaction
     * @param queries List of queries to execute
     * @return Success if all queries succeed and transaction commits, error otherwise
     *
     * This is a convenience method that automatically begins, executes, and commits/rollbacks.
     */
    kcenon::common::VoidResult execute_transaction(
        const std::vector<std::string>& queries);

    /**
     * @brief Execute a function within a transaction
     * @param func Function to execute within transaction
     * @return Result of the function or transaction error
     *
     * @example
     * auto result = db.in_transaction([&](transaction& tx) {
     *     tx.execute("INSERT INTO users (name) VALUES ($1)", {"Alice"});
     *     tx.execute("UPDATE accounts SET balance = balance - 100");
     *     return true;
     * });
     */
    template<typename Func>
    kcenon::common::Result<typename std::invoke_result_t<Func, transaction&>> in_transaction(Func&& func);

    // Query builder integration

    /**
     * @brief Create a query builder for this database
     * @return Query builder instance
     *
     * @example
     * auto builder = db.create_query_builder();
     * builder.select("name, email")
     *     .from("users")
     *     .where("age", ">", 18)
     *     .order_by("name")
     *     .limit(10);
     * auto query_str = builder.build();
     * auto users = db.execute(query_str);
     */
    query_builder create_query_builder() const;

    // Monitoring and health

    /**
     * @brief Get current performance metrics
     * @return Database performance metrics
     */
    database_metrics get_metrics() const;

    /**
     * @brief Perform health check
     * @return Health check results
     */
    health_check check_health() const;

    /**
     * @brief Reset metrics counters
     */
    void reset_metrics();

    // Configuration access

    /**
     * @brief Get current configuration
     * @return Database configuration
     */
    const unified_db_config& get_config() const;

    /**
     * @brief Get database backend type
     * @return Backend type
     */
    backend_type get_backend_type() const;

    /**
     * @brief Get connection pool statistics
     * @return Pool statistics
     */
    struct pool_stats {
        size_t total_connections{0};
        size_t active_connections{0};
        size_t idle_connections{0};
        size_t wait_queue_size{0};
        double utilization_percent{0.0};
    };
    pool_stats get_pool_stats() const;

private:
    // PIMPL idiom for ABI stability
    class impl;
    std::unique_ptr<impl> pimpl_;
};

/**
 * @brief Convenience alias for unified database system
 */
using database = unified_database_system;

/**
 * @brief Create a database with zero configuration
 * @return Unique pointer to database system
 */
inline std::unique_ptr<unified_database_system> create_database() {
    return std::make_unique<unified_database_system>();
}

/**
 * @brief Create a database with builder configuration
 * @param backend Database backend type
 * @param connection_string Connection string
 * @return Result containing configured database system or error
 */
inline kcenon::common::Result<std::unique_ptr<unified_database_system>> create_database(
    backend_type backend,
    const std::string& connection_string) {

    return unified_database_system::create_builder()
        .set_backend(backend)
        .set_connection_string(connection_string)
        .build();
}

} // namespace database::integrated
