/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "sql_dialect.h"
#include <stdexcept>

namespace database::query {

// Factory method
std::unique_ptr<sql_dialect> sql_dialect::create(database_types type) {
    switch (type) {
        case database_types::PostgreSQL:
            return std::make_unique<postgresql_dialect>();
        case database_types::MySQL:
        case database_types::MariaDB:
            return std::make_unique<mysql_dialect>();
        case database_types::SQLite:
            return std::make_unique<sqlite_dialect>();
        default:
            throw std::invalid_argument("Unsupported database type for SQL dialect");
    }
}

// PostgreSQL Dialect Implementation

std::string postgresql_dialect::limit_clause(size_t limit, size_t offset) const {
    std::string clause = "LIMIT " + std::to_string(limit);
    if (offset > 0) {
        clause += " OFFSET " + std::to_string(offset);
    }
    return clause;
}

std::string postgresql_dialect::auto_increment() const {
    return "SERIAL";
}

std::string postgresql_dialect::current_timestamp() const {
    return "CURRENT_TIMESTAMP";
}

std::string postgresql_dialect::concat_operator() const {
    return "||";
}

bool postgresql_dialect::supports_feature(const std::string& feature) const {
    // PostgreSQL supports most SQL features
    if (feature == "window_functions") return true;
    if (feature == "cte") return true;  // Common Table Expressions
    if (feature == "recursive_cte") return true;
    if (feature == "json") return true;
    if (feature == "full_outer_join") return true;
    if (feature == "returning") return true;  // RETURNING clause
    if (feature == "array") return true;
    if (feature == "upsert") return true;  // ON CONFLICT
    return false;
}

// MySQL Dialect Implementation

std::string mysql_dialect::limit_clause(size_t limit, size_t offset) const {
    if (offset > 0) {
        // MySQL uses "LIMIT offset, limit" syntax
        return "LIMIT " + std::to_string(offset) + ", " + std::to_string(limit);
    }
    return "LIMIT " + std::to_string(limit);
}

std::string mysql_dialect::auto_increment() const {
    return "AUTO_INCREMENT";
}

std::string mysql_dialect::current_timestamp() const {
    return "NOW()";
}

std::string mysql_dialect::concat_operator() const {
    return "CONCAT";  // MySQL uses CONCAT function
}

bool mysql_dialect::supports_feature(const std::string& feature) const {
    if (feature == "window_functions") return true;  // MySQL 8.0+
    if (feature == "cte") return true;  // MySQL 8.0+
    if (feature == "recursive_cte") return true;  // MySQL 8.0+
    if (feature == "json") return true;
    if (feature == "full_outer_join") return false;  // Not supported
    if (feature == "returning") return false;  // Not supported
    if (feature == "array") return false;  // Not supported
    if (feature == "upsert") return true;  // ON DUPLICATE KEY UPDATE
    return false;
}

// SQLite Dialect Implementation

std::string sqlite_dialect::limit_clause(size_t limit, size_t offset) const {
    std::string clause = "LIMIT " + std::to_string(limit);
    if (offset > 0) {
        clause += " OFFSET " + std::to_string(offset);
    }
    return clause;
}

std::string sqlite_dialect::auto_increment() const {
    return "AUTOINCREMENT";
}

std::string sqlite_dialect::current_timestamp() const {
    return "CURRENT_TIMESTAMP";
}

std::string sqlite_dialect::concat_operator() const {
    return "||";
}

bool sqlite_dialect::supports_feature(const std::string& feature) const {
    if (feature == "window_functions") return true;  // SQLite 3.25+
    if (feature == "cte") return true;
    if (feature == "recursive_cte") return true;
    if (feature == "json") return true;  // SQLite 3.38+
    if (feature == "full_outer_join") return false;  // Not supported
    if (feature == "returning") return true;  // SQLite 3.35+
    if (feature == "array") return false;  // Not supported
    if (feature == "upsert") return true;  // ON CONFLICT
    return false;
}

} // namespace database::query
