/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "../database_types.h"
#include <string>
#include <memory>

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
 * - LIMIT/OFFSET clause formatting
 * - Auto-increment column definition
 * - Current timestamp functions
 * - Boolean literals
 * - Data type mappings
 */
class sql_dialect {
public:
    virtual ~sql_dialect() = default;

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
    std::string limit_clause(size_t limit, size_t offset) const override;
    std::string auto_increment() const override;
    std::string current_timestamp() const override;
    std::string concat_operator() const override;
    bool supports_feature(const std::string& feature) const override;
};

} // namespace database::query
