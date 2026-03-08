/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "value_formatter.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <variant>

namespace database::query {

value_formatter::value_formatter(database_types db_type)
    : db_type_(db_type) {}

std::string value_formatter::format(const core::database_value& value) const {
    return std::visit([this](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return null_literal();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return format_string(arg);
        }
        else if constexpr (std::is_same_v<T, int>) {
            return format_int(arg);
        }
        else if constexpr (std::is_same_v<T, int64_t>) {
            return format_int(arg);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return format_double(arg);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return format_bool(arg);
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return format_blob(arg);
        }
        else {
            return null_literal();
        }
    }, value);
}

std::string value_formatter::escape_string(const std::string& str) const {
    switch (db_type_) {
        case database_types::postgres:
            return escape_postgresql_string(str);
        case database_types::sqlite:
            return escape_sqlite_string(str);
        case database_types::mongodb:
        case database_types::redis:
            // NoSQL databases handle escaping differently
            return str;
        default:
            return escape_sqlite_string(str);  // Safe default
    }
}

std::string value_formatter::escape_identifier(const std::string& identifier) const {
    switch (db_type_) {
        case database_types::postgres:
            return "\"" + identifier + "\"";
        case database_types::sqlite:
            return "\"" + identifier + "\"";
        case database_types::mongodb:
        case database_types::redis:
            return identifier;  // NoSQL doesn't quote identifiers
        default:
            return "\"" + identifier + "\"";
    }
}

std::string value_formatter::null_literal() const {
    return "NULL";
}

std::string value_formatter::bool_literal(bool val) const {
    switch (db_type_) {
        case database_types::postgres:
        case database_types::sqlite:
            return val ? "TRUE" : "FALSE";
        case database_types::mongodb:
            return val ? "true" : "false";
        case database_types::redis:
            return val ? "1" : "0";
        default:
            return val ? "1" : "0";
    }
}

// Private formatting methods

std::string value_formatter::format_string(const std::string& str) const {
    return "'" + escape_string(str) + "'";
}

std::string value_formatter::format_int(int64_t num) const {
    return std::to_string(num);
}

std::string value_formatter::format_double(double num) const {
    // Handle special values
    if (std::isnan(num)) {
        return "'NaN'";
    }
    if (std::isinf(num)) {
        return num > 0 ? "'Infinity'" : "'-Infinity'";
    }

    // Format with appropriate precision
    std::ostringstream oss;
    oss << std::setprecision(15) << num;
    return oss.str();
}

std::string value_formatter::format_bool(bool val) const {
    return bool_literal(val);
}

std::string value_formatter::format_blob(const std::vector<uint8_t>& data) const {
    switch (db_type_) {
        case database_types::postgres: {
            // PostgreSQL hex format: '\x...'
            std::ostringstream oss;
            oss << "'\\x";
            for (uint8_t byte : data) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(byte);
            }
            oss << "'";
            return oss.str();
        }
        case database_types::sqlite: {
            // SQLite hex format: X'...'
            std::ostringstream oss;
            oss << "X'";
            for (uint8_t byte : data) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(byte);
            }
            oss << "'";
            return oss.str();
        }
        default:
            return "NULL";  // Unsupported for this database
    }
}

// Database-specific string escaping

std::string value_formatter::escape_postgresql_string(const std::string& str) const {
    std::string result;
    result.reserve(str.length() + 10);

    for (char c : str) {
        switch (c) {
            case '\'':
                result += "''";  // PostgreSQL escapes single quotes by doubling
                break;
            case '\\':
                result += "\\\\";  // Escape backslashes
                break;
            case '\0':
                result += "\\0";  // NULL byte
                break;
            case '\n':
                result += "\\n";  // Newline
                break;
            case '\r':
                result += "\\r";  // Carriage return
                break;
            case '\t':
                result += "\\t";  // Tab
                break;
            default:
                result += c;
        }
    }

    return result;
}

std::string value_formatter::escape_sqlite_string(const std::string& str) const {
    std::string result;
    result.reserve(str.length() + 10);

    for (char c : str) {
        if (c == '\'') {
            result += "''";  // SQLite escapes single quotes by doubling
        } else {
            result += c;
        }
    }

    return result;
}

} // namespace database::query
