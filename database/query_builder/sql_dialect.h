/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "../database_types.h"
#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace database::query {

/**
 * @class sql_dialect
 * @brief Abstract base class for database-specific SQL dialects
 *
 * Strategy Pattern:
 * - Encapsulates database-specific SQL syntax variations
 * - Allows runtime selection of appropriate dialect
 * - Makes it easy to add new database support
 *
 * Responsibilities:
 * - Placeholder style formatting ($1 vs ? vs ?1)
 * - Identifier quoting ("col" vs `col` vs [col])
 * - LIMIT/OFFSET clause formatting
 * - RETURNING clause support
 * - UPSERT clause formatting
 * - Auto-increment column definition
 * - Current timestamp functions
 * - String escaping
 * - Boolean literals
 * - Data type mappings
 */
class sql_dialect {
public:
    virtual ~sql_dialect() = default;

    /**
     * @brief Generate parameter placeholder
     * @param index 1-based parameter index
     * @return Database-specific placeholder string
     *
     * Examples:
     * - PostgreSQL: "$1", "$2", "$3"
     * - MySQL: "?"
     * - SQLite: "?1", "?2", "?3"
     */
    virtual std::string placeholder(int index) const = 0;

    /**
     * @brief Quote an identifier (table or column name)
     * @param name The identifier to quote
     * @return Quoted identifier
     *
     * Examples:
     * - PostgreSQL: "\"column_name\""
     * - MySQL: "`column_name`"
     * - SQLite: "\"column_name\""
     */
    virtual std::string quote_identifier(std::string_view name) const = 0;

    /**
     * @brief Escape a string value for safe SQL inclusion
     * @param str The string to escape
     * @return Escaped string (without surrounding quotes)
     */
    virtual std::string escape_string(std::string_view str) const = 0;

    /**
     * @brief Generate RETURNING clause for INSERT/UPDATE
     * @param column The column to return (empty for all)
     * @return Database-specific RETURNING clause or empty if unsupported
     *
     * Examples:
     * - PostgreSQL: " RETURNING id"
     * - MySQL: "" (not supported, use LAST_INSERT_ID())
     * - SQLite: " RETURNING id" (3.35+)
     */
    virtual std::string returning_clause(std::string_view column = "") const = 0;

    /**
     * @brief Generate UPSERT (INSERT OR UPDATE) clause
     * @param conflict_columns Columns that define conflict
     * @param update_columns Columns to update on conflict
     * @return Database-specific upsert clause
     *
     * Examples:
     * - PostgreSQL: "ON CONFLICT (id) DO UPDATE SET ..."
     * - MySQL: "ON DUPLICATE KEY UPDATE ..."
     * - SQLite: "ON CONFLICT (id) DO UPDATE SET ..."
     */
    virtual std::string upsert_clause(
        const std::vector<std::string>& conflict_columns,
        const std::vector<std::string>& update_columns) const = 0;

    /**
     * @brief Generate LIMIT clause
     * @param limit Maximum number of rows
     * @param offset Number of rows to skip
     * @return Database-specific LIMIT clause
     *
     * Examples:
     * - PostgreSQL: "LIMIT 10 OFFSET 5"
     * - MySQL: "LIMIT 5, 10"
     * - SQLite: "LIMIT 10 OFFSET 5"
     */
    virtual std::string limit_clause(size_t limit, size_t offset) const = 0;

    /**
     * @brief Get auto-increment column definition
     * @return SQL fragment for auto-increment column
     *
     * Examples:
     * - PostgreSQL: "SERIAL"
     * - MySQL: "AUTO_INCREMENT"
     * - SQLite: "AUTOINCREMENT"
     */
    virtual std::string auto_increment() const = 0;

    /**
     * @brief Get current timestamp function
     * @return SQL function for current timestamp
     *
     * Examples:
     * - PostgreSQL: "CURRENT_TIMESTAMP"
     * - MySQL: "NOW()"
     * - SQLite: "CURRENT_TIMESTAMP"
     */
    virtual std::string current_timestamp() const = 0;

    /**
     * @brief Get string concatenation operator
     * @return Operator or function for concatenation
     *
     * Examples:
     * - PostgreSQL: "||"
     * - MySQL: "CONCAT"
     * - SQLite: "||"
     */
    virtual std::string concat_operator() const = 0;

    /**
     * @brief Check if database supports specific feature
     * @param feature Feature name
     * @return true if supported
     */
    virtual bool supports_feature(const std::string& feature) const = 0;

    /**
     * @brief Factory method to create appropriate dialect
     * @param type Database type
     * @return Unique pointer to dialect instance
     */
    static std::unique_ptr<sql_dialect> create(database_types type);
};

/**
 * @class postgresql_dialect
 * @brief PostgreSQL-specific SQL dialect
 */
class postgresql_dialect : public sql_dialect {
public:
    std::string placeholder(int index) const override;
    std::string quote_identifier(std::string_view name) const override;
    std::string escape_string(std::string_view str) const override;
    std::string returning_clause(std::string_view column = "") const override;
    std::string upsert_clause(
        const std::vector<std::string>& conflict_columns,
        const std::vector<std::string>& update_columns) const override;
    std::string limit_clause(size_t limit, size_t offset) const override;
    std::string auto_increment() const override;
    std::string current_timestamp() const override;
    std::string concat_operator() const override;
    bool supports_feature(const std::string& feature) const override;
};

/**
 * @class mysql_dialect
 * @brief MySQL-specific SQL dialect
 */
class mysql_dialect : public sql_dialect {
public:
    std::string placeholder(int index) const override;
    std::string quote_identifier(std::string_view name) const override;
    std::string escape_string(std::string_view str) const override;
    std::string returning_clause(std::string_view column = "") const override;
    std::string upsert_clause(
        const std::vector<std::string>& conflict_columns,
        const std::vector<std::string>& update_columns) const override;
    std::string limit_clause(size_t limit, size_t offset) const override;
    std::string auto_increment() const override;
    std::string current_timestamp() const override;
    std::string concat_operator() const override;
    bool supports_feature(const std::string& feature) const override;
};

/**
 * @class sqlite_dialect
 * @brief SQLite-specific SQL dialect
 */
class sqlite_dialect : public sql_dialect {
public:
    std::string placeholder(int index) const override;
    std::string quote_identifier(std::string_view name) const override;
    std::string escape_string(std::string_view str) const override;
    std::string returning_clause(std::string_view column = "") const override;
    std::string upsert_clause(
        const std::vector<std::string>& conflict_columns,
        const std::vector<std::string>& update_columns) const override;
    std::string limit_clause(size_t limit, size_t offset) const override;
    std::string auto_increment() const override;
    std::string current_timestamp() const override;
    std::string concat_operator() const override;
    bool supports_feature(const std::string& feature) const override;
};

} // namespace database::query
