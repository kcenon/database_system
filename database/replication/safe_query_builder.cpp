/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "safe_query_builder.h"

namespace database::replication {

// Valid identifier pattern: alphanumeric, underscore, dot (for schema.table)
const std::regex safe_query_builder::VALID_IDENTIFIER_PATTERN(
    "^[a-zA-Z_][a-zA-Z0-9_.]*$"
);

std::string safe_query_builder::escape_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);  // Reserve extra space for escaping

    for (char c : value) {
        if (c == '\'') {
            escaped += "''";  // Double single quotes
        } else if (c == '\\') {
            escaped += "\\\\";  // Escape backslashes
        } else if (c == '\0') {
            // Skip null characters (potential injection vector)
            continue;
        } else {
            escaped += c;
        }
    }

    return escaped;
}

std::string safe_query_builder::escape_identifier(const std::string& identifier) {
    if (identifier.empty()) {
        throw std::invalid_argument("Identifier cannot be empty");
    }

    if (!is_valid_identifier(identifier)) {
        throw std::invalid_argument(
            "Invalid identifier: '" + identifier + "'. "
            "Only alphanumeric characters, underscores, and dots are allowed."
        );
    }

    // Wrap in double quotes for standard SQL
    return "\"" + identifier + "\"";
}

bool safe_query_builder::is_valid_identifier(const std::string& identifier) {
    if (identifier.empty() || identifier.length() > 128) {
        return false;
    }

    return std::regex_match(identifier, VALID_IDENTIFIER_PATTERN);
}

std::string safe_query_builder::build_insert(
    const std::string& table_name,
    const std::unordered_map<std::string, std::string>& values
) {
    if (table_name.empty()) {
        throw std::invalid_argument("Table name cannot be empty");
    }

    if (values.empty()) {
        throw std::invalid_argument("Values cannot be empty for INSERT");
    }

    std::ostringstream query;
    std::ostringstream columns;
    std::ostringstream vals;

    query << "INSERT INTO " << escape_identifier(table_name) << " (";

    bool first = true;
    for (const auto& [col, val] : values) {
        if (!first) {
            columns << ", ";
            vals << ", ";
        }
        columns << escape_identifier(col);
        vals << "'" << escape_value(val) << "'";
        first = false;
    }

    query << columns.str() << ") VALUES (" << vals.str() << ")";

    return query.str();
}

std::string safe_query_builder::build_update(
    const std::string& table_name,
    const std::unordered_map<std::string, std::string>& new_values,
    const std::unordered_map<std::string, std::string>& where_values
) {
    if (table_name.empty()) {
        throw std::invalid_argument("Table name cannot be empty");
    }

    if (new_values.empty()) {
        throw std::invalid_argument("New values cannot be empty for UPDATE");
    }

    if (where_values.empty()) {
        throw std::invalid_argument("WHERE clause cannot be empty for UPDATE (safety measure)");
    }

    std::ostringstream query;
    query << "UPDATE " << escape_identifier(table_name) << " SET ";

    // Build SET clause
    bool first = true;
    for (const auto& [col, val] : new_values) {
        if (!first) {
            query << ", ";
        }
        query << escape_identifier(col) << " = '" << escape_value(val) << "'";
        first = false;
    }

    // Build WHERE clause
    query << " WHERE ";
    first = true;
    for (const auto& [col, val] : where_values) {
        if (!first) {
            query << " AND ";
        }
        query << escape_identifier(col) << " = '" << escape_value(val) << "'";
        first = false;
    }

    return query.str();
}

std::string safe_query_builder::build_delete(
    const std::string& table_name,
    const std::unordered_map<std::string, std::string>& where_values
) {
    if (table_name.empty()) {
        throw std::invalid_argument("Table name cannot be empty");
    }

    if (where_values.empty()) {
        throw std::invalid_argument("WHERE clause cannot be empty for DELETE (safety measure)");
    }

    std::ostringstream query;
    query << "DELETE FROM " << escape_identifier(table_name) << " WHERE ";

    bool first = true;
    for (const auto& [col, val] : where_values) {
        if (!first) {
            query << " AND ";
        }
        query << escape_identifier(col) << " = '" << escape_value(val) << "'";
        first = false;
    }

    return query.str();
}

} // namespace database::replication
