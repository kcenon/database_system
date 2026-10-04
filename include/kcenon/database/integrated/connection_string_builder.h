// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_string_builder.h
 * @brief Fluent builder for constructing type-safe database connection strings
 *
 * Provides a type-safe, discoverable way to construct connection strings for
 * various database backends instead of manually formatting strings.
 *
 * @example
 * @code
 * using namespace kcenon::database::integrated;
 *
 * // PostgreSQL connection string
 * auto pg_conn = connection_string_builder()
 *     .host("localhost")
 *     .port(5432)
 *     .database("mydb")
 *     .user("admin")
 *     .password("secret")
 *     .ssl_mode(ssl_mode::require)
 *     .build(backend_type::postgres);
 * // Result: "host=localhost port=5432 dbname=mydb user=admin password=secret sslmode=require"
 *
 * // SQLite connection string
 * auto sqlite_conn = connection_string_builder()
 *     .database("mydb.db")
 *     .build(backend_type::sqlite);
 * // Result: "mydb.db"
 *
 * // SQLite in-memory database
 * auto sqlite_mem = connection_string_builder()
 *     .in_memory()
 *     .build(backend_type::sqlite);
 * // Result: ":memory:"
 * @endcode
 */

#pragma once

#include <kcenon/database/integrated/core/configuration.h>
#include <kcenon/database/core/result.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::database::integrated {

/**
 * @brief SSL connection mode for database connections
 */
enum class ssl_mode {
    disable,   ///< No SSL
    allow,     ///< Try SSL, fall back to non-SSL
    prefer,    ///< Try SSL first, fall back to non-SSL
    require,   ///< Require SSL, no verification
    verify_ca, ///< Require SSL with CA verification
    verify_full ///< Require SSL with full verification
};

/**
 * @brief Fluent builder for constructing database connection strings
 *
 * This class provides a type-safe way to build connection strings for
 * different database backends. It validates required fields per backend
 * and generates properly formatted connection strings.
 *
 * **Thread Safety:** This class is not thread-safe. Each thread should
 * use its own builder instance.
 */
class connection_string_builder {
public:
    connection_string_builder() = default;
    ~connection_string_builder() = default;

    // Copy and move
    connection_string_builder(const connection_string_builder&) = default;
    connection_string_builder& operator=(const connection_string_builder&) = default;
    connection_string_builder(connection_string_builder&&) noexcept = default;
    connection_string_builder& operator=(connection_string_builder&&) noexcept = default;

    /**
     * @brief Set the database host
     * @param h Hostname or IP address
     * @return Reference to this builder for chaining
     */
    connection_string_builder& host(std::string_view h);

    /**
     * @brief Set the database port
     * @param p Port number (1-65535)
     * @return Reference to this builder for chaining
     */
    connection_string_builder& port(uint16_t p);

    /**
     * @brief Set the database name
     * @param db Database name or file path (for SQLite)
     * @return Reference to this builder for chaining
     */
    connection_string_builder& database(std::string_view db);

    /**
     * @brief Set the username for authentication
     * @param u Username
     * @return Reference to this builder for chaining
     */
    connection_string_builder& user(std::string_view u);

    /**
     * @brief Set the password for authentication
     * @param p Password
     * @return Reference to this builder for chaining
     */
    connection_string_builder& password(std::string_view p);

    /**
     * @brief Set the SSL connection mode
     * @param mode SSL mode
     * @return Reference to this builder for chaining
     */
    connection_string_builder& ssl_mode(enum ssl_mode mode);

    /**
     * @brief Set the connection timeout
     * @param seconds Timeout in seconds
     * @return Reference to this builder for chaining
     */
    connection_string_builder& connect_timeout(uint32_t seconds);

    /**
     * @brief Set the application name (for PostgreSQL)
     * @param name Application name
     * @return Reference to this builder for chaining
     */
    connection_string_builder& application_name(std::string_view name);

    /**
     * @brief Configure SQLite to use in-memory database
     * @return Reference to this builder for chaining
     */
    connection_string_builder& in_memory();

    /**
     * @brief Add a custom option
     * @param key Option key
     * @param value Option value
     * @return Reference to this builder for chaining
     */
    connection_string_builder& option(std::string_view key, std::string_view value);

    /**
     * @brief Build the connection string for the specified backend
     * @param type Database backend type
     * @return Result containing the connection string or validation error
     *
     * Validates that required fields are set for the backend:
     * - PostgreSQL: host is recommended (defaults to localhost)
     * - SQLite: database path is required (or in_memory must be set)
     * - MongoDB: host is required
     * - Redis: host is required
     */
    [[nodiscard]] kcenon::common::Result<std::string> build(backend_type type) const;

    /**
     * @brief Reset the builder to initial state
     * @return Reference to this builder for chaining
     */
    connection_string_builder& reset();

private:
    std::optional<std::string> host_;
    std::optional<uint16_t> port_;
    std::optional<std::string> database_;
    std::optional<std::string> user_;
    std::optional<std::string> password_;
    std::optional<enum ssl_mode> ssl_mode_;
    std::optional<uint32_t> connect_timeout_;
    std::optional<std::string> application_name_;
    bool in_memory_ = false;
    std::vector<std::pair<std::string, std::string>> custom_options_;

    [[nodiscard]] kcenon::common::Result<std::string> build_postgres() const;
    [[nodiscard]] kcenon::common::Result<std::string> build_sqlite() const;
    [[nodiscard]] kcenon::common::Result<std::string> build_mongodb() const;
    [[nodiscard]] kcenon::common::Result<std::string> build_redis() const;

    [[nodiscard]] static std::string ssl_mode_to_postgres_string(enum ssl_mode mode);
};

} // namespace kcenon::database::integrated
