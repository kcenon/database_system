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
        case database_types::postgres:
            return std::make_unique<postgresql_dialect>();
        case database_types::sqlite:
            return std::make_unique<sqlite_dialect>();
        default:
            throw std::invalid_argument("Unsupported database type for SQL dialect");
    }
}

// PostgreSQL Dialect Implementation

std::string postgresql_dialect::placeholder(int index) const {
    return "$" + std::to_string(index);
}

std::string postgresql_dialect::quote_identifier(std::string_view name) const {
    std::string result;
    result.reserve(name.size() + 2);
    result += '"';
    for (char c : name) {
        if (c == '"') {
            result += "\"\"";  // Escape double quotes by doubling
        } else {
            result += c;
        }
    }
    result += '"';
    return result;
}

std::string postgresql_dialect::escape_string(std::string_view str) const {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (c == '\'') {
            result += "''";  // Escape single quotes by doubling
        } else if (c == '\\') {
            result += "\\\\";  // Escape backslashes
        } else {
            result += c;
        }
    }
    return result;
}

std::string postgresql_dialect::returning_clause(std::string_view column) const {
    if (column.empty()) {
        return " RETURNING *";
    }
    return " RETURNING " + quote_identifier(column);
}

std::string postgresql_dialect::upsert_clause(
    const std::vector<std::string>& conflict_columns,
    const std::vector<std::string>& update_columns) const {

    if (conflict_columns.empty()) {
        return "";
    }

    std::string result = "ON CONFLICT (";
    for (size_t i = 0; i < conflict_columns.size(); ++i) {
        if (i > 0) result += ", ";
        result += quote_identifier(conflict_columns[i]);
    }
    result += ")";

    if (update_columns.empty()) {
        result += " DO NOTHING";
    } else {
        result += " DO UPDATE SET ";
        for (size_t i = 0; i < update_columns.size(); ++i) {
            if (i > 0) result += ", ";
            std::string quoted = quote_identifier(update_columns[i]);
            result += quoted + " = EXCLUDED." + quoted;
        }
    }
    return result;
}

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

// SQLite Dialect Implementation

std::string sqlite_dialect::placeholder(int index) const {
    return "?" + std::to_string(index);
}

std::string sqlite_dialect::quote_identifier(std::string_view name) const {
    std::string result;
    result.reserve(name.size() + 2);
    result += '"';
    for (char c : name) {
        if (c == '"') {
            result += "\"\"";  // Escape double quotes by doubling
        } else {
            result += c;
        }
    }
    result += '"';
    return result;
}

std::string sqlite_dialect::escape_string(std::string_view str) const {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (c == '\'') {
            result += "''";  // Escape single quotes by doubling
        } else {
            result += c;
        }
    }
    return result;
}

std::string sqlite_dialect::returning_clause(std::string_view column) const {
    // SQLite 3.35+ supports RETURNING
    if (column.empty()) {
        return " RETURNING *";
    }
    return " RETURNING " + quote_identifier(column);
}

std::string sqlite_dialect::upsert_clause(
    const std::vector<std::string>& conflict_columns,
    const std::vector<std::string>& update_columns) const {

    if (conflict_columns.empty()) {
        return "";
    }

    std::string result = "ON CONFLICT (";
    for (size_t i = 0; i < conflict_columns.size(); ++i) {
        if (i > 0) result += ", ";
        result += quote_identifier(conflict_columns[i]);
    }
    result += ")";

    if (update_columns.empty()) {
        result += " DO NOTHING";
    } else {
        result += " DO UPDATE SET ";
        for (size_t i = 0; i < update_columns.size(); ++i) {
            if (i > 0) result += ", ";
            std::string quoted = quote_identifier(update_columns[i]);
            result += quoted + " = excluded." + quoted;
        }
    }
    return result;
}

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
