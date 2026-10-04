// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/core/database_backend.h>
#include <string>
#include <vector>

namespace kcenon::database::query {

/**
 * @class value_formatter
 * @brief Formats database values for different backends
 *
 * Responsibilities:
 * - Format different data types (string, int, double, bool, blob, null)
 * - Database-specific escaping (PostgreSQL, SQLite)
 * - Identifier quoting
 * - NULL literal formatting
 *
 * Single Responsibility Principle:
 * - This class only handles value formatting and escaping
 * - Does not handle query building or condition logic
 *
 * Thread Safety:
 * - Thread-safe after construction (immutable state)
 * - Can be shared across threads
 */
class value_formatter {
public:
    /**
     * @brief Construct formatter for specific database type
     * @param db_type Target database type
     */
    explicit value_formatter(database_types db_type);

    /**
     * @brief Format a database value
     * @param value Value to format
     * @return Formatted string representation
     */
    std::string format(const core::database_value& value) const;

    /**
     * @brief Escape a string value
     * @param str String to escape
     * @return Escaped string (without quotes)
     */
    std::string escape_string(const std::string& str) const;

    /**
     * @brief Quote and escape an identifier (table/column name)
     * @param identifier Identifier to escape
     * @return Quoted and escaped identifier
     */
    std::string escape_identifier(const std::string& identifier) const;

    /**
     * @brief Get NULL literal for this database
     * @return "NULL" for most databases
     */
    std::string null_literal() const;

    /**
     * @brief Get boolean literal
     * @param val Boolean value
     * @return Database-specific boolean representation
     */
    std::string bool_literal(bool val) const;

private:
    database_types db_type_;

    std::string format_string(const std::string& str) const;
    std::string format_int(int64_t num) const;
    std::string format_double(double num) const;
    std::string format_bool(bool val) const;
    std::string format_blob(const std::vector<uint8_t>& data) const;

    // Database-specific escaping
    std::string escape_postgresql_string(const std::string& str) const;
    std::string escape_sqlite_string(const std::string& str) const;
};

} // namespace kcenon::database::query
