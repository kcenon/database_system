/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
#include <regex>
#include <stdexcept>

namespace database::replication {

/**
 * @class safe_query_builder
 * @brief Builds SQL queries with proper value escaping to prevent SQL injection
 *
 * This utility class provides safe SQL query construction for replication events.
 * All string values are properly escaped to prevent SQL injection attacks.
 *
 * Thread Safety:
 * - NOT thread-safe. Each thread should use its own instance.
 *
 * Example Usage:
 * @code
 *   safe_query_builder builder;
 *   auto query = builder.build_insert("users", {{"name", "O'Brien"}, {"age", "30"}});
 *   // Result: INSERT INTO "users" ("name", "age") VALUES ('O''Brien', '30')
 * @endcode
 */
class safe_query_builder {
public:
    /**
     * @brief Build a safe INSERT query
     * @param table_name Table name (will be escaped as identifier)
     * @param values Column-value pairs to insert
     * @return Safe SQL INSERT query string
     * @throws std::invalid_argument if table_name or values are empty
     */
    static std::string build_insert(
        const std::string& table_name,
        const std::unordered_map<std::string, std::string>& values
    );

    /**
     * @brief Build a safe UPDATE query
     * @param table_name Table name (will be escaped as identifier)
     * @param new_values Column-value pairs for SET clause
     * @param where_values Column-value pairs for WHERE clause
     * @return Safe SQL UPDATE query string
     * @throws std::invalid_argument if table_name or values are empty
     */
    static std::string build_update(
        const std::string& table_name,
        const std::unordered_map<std::string, std::string>& new_values,
        const std::unordered_map<std::string, std::string>& where_values
    );

    /**
     * @brief Build a safe DELETE query
     * @param table_name Table name (will be escaped as identifier)
     * @param where_values Column-value pairs for WHERE clause
     * @return Safe SQL DELETE query string
     * @throws std::invalid_argument if table_name or where_values are empty
     */
    static std::string build_delete(
        const std::string& table_name,
        const std::unordered_map<std::string, std::string>& where_values
    );

    /**
     * @brief Escape a string value for safe SQL inclusion
     * @param value The string value to escape
     * @return Escaped string with single quotes doubled
     *
     * This method doubles single quotes to prevent SQL injection:
     * - Input: O'Brien -> Output: O''Brien
     * - Input: '; DROP TABLE users; -- -> Output: ''; DROP TABLE users; --
     */
    static std::string escape_value(const std::string& value);

    /**
     * @brief Escape an identifier (table name, column name) for safe SQL inclusion
     * @param identifier The identifier to escape
     * @return Escaped identifier wrapped in double quotes
     * @throws std::invalid_argument if identifier contains invalid characters
     *
     * Identifiers are validated to only contain alphanumeric characters,
     * underscores, and dots (for schema.table notation).
     */
    static std::string escape_identifier(const std::string& identifier);

    /**
     * @brief Validate that an identifier is safe
     * @param identifier The identifier to validate
     * @return true if identifier is safe, false otherwise
     */
    static bool is_valid_identifier(const std::string& identifier);

private:
    // Regex pattern for valid identifiers: alphanumeric, underscore, dot
    static const std::regex VALID_IDENTIFIER_PATTERN;
};

} // namespace database::replication
