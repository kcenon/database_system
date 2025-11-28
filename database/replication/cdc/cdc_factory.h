/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include "cdc_strategy_interface.h"
#include "sqlite_cdc_strategy.h"

#include <memory>
#include <string>

namespace database::replication::cdc {

/**
 * @class cdc_factory
 * @brief Factory class for creating database-specific CDC strategies
 *
 * This factory creates the appropriate CDC strategy based on the database type.
 * Currently supports SQLite with placeholder implementations for PostgreSQL,
 * MySQL, and MongoDB.
 *
 * Example:
 * @code
 *   // Create SQLite CDC strategy
 *   auto sqlite_cdc = cdc_factory::create(database_type::SQLITE);
 *
 *   // Create from connection string
 *   auto cdc = cdc_factory::create_from_connection_string(
 *       "sqlite:///path/to/database.db"
 *   );
 * @endcode
 */
class cdc_factory {
public:
    /**
     * @brief Create a CDC strategy for the specified database type
     * @param type Database type
     * @return Unique pointer to CDC strategy, or nullptr if unsupported
     */
    static std::unique_ptr<cdc_strategy_interface> create(database_type type);

    /**
     * @brief Create a CDC strategy from a connection string
     * @param connection_string Database connection string
     * @return Unique pointer to CDC strategy, or nullptr if type cannot be determined
     *
     * Connection string formats:
     * - SQLite: "sqlite:///path/to/database.db" or just "/path/to/database.db"
     * - PostgreSQL: "postgresql://user:pass@host:port/dbname"
     * - MySQL: "mysql://user:pass@host:port/dbname"
     * - MongoDB: "mongodb://user:pass@host:port/dbname"
     */
    static std::unique_ptr<cdc_strategy_interface> create_from_connection_string(
        const std::string& connection_string
    );

    /**
     * @brief Detect database type from connection string
     * @param connection_string Database connection string
     * @return Detected database type
     */
    static database_type detect_database_type(const std::string& connection_string);

    /**
     * @brief Check if a database type is supported
     * @param type Database type
     * @return true if supported
     */
    static bool is_supported(database_type type);

    /**
     * @brief Get human-readable name for database type
     * @param type Database type
     * @return Database type name
     */
    static std::string get_type_name(database_type type);
};

// Implementation

inline std::unique_ptr<cdc_strategy_interface> cdc_factory::create(database_type type) {
    switch (type) {
        case database_type::SQLITE:
            return std::make_unique<sqlite_cdc_strategy>();

        case database_type::POSTGRESQL:
            // TODO: Implement PostgreSQL CDC strategy using logical replication
            return nullptr;

        case database_type::MYSQL:
            // TODO: Implement MySQL CDC strategy using binary log
            return nullptr;

        case database_type::MONGODB:
            // TODO: Implement MongoDB CDC strategy using change streams
            return nullptr;

        default:
            return nullptr;
    }
}

inline std::unique_ptr<cdc_strategy_interface> cdc_factory::create_from_connection_string(
    const std::string& connection_string
) {
    auto type = detect_database_type(connection_string);
    return create(type);
}

inline database_type cdc_factory::detect_database_type(const std::string& connection_string) {
    // Check for explicit protocol prefixes
    if (connection_string.find("sqlite://") == 0 ||
        connection_string.find("sqlite:") == 0) {
        return database_type::SQLITE;
    }

    if (connection_string.find("postgresql://") == 0 ||
        connection_string.find("postgres://") == 0) {
        return database_type::POSTGRESQL;
    }

    if (connection_string.find("mysql://") == 0) {
        return database_type::MYSQL;
    }

    if (connection_string.find("mongodb://") == 0 ||
        connection_string.find("mongodb+srv://") == 0) {
        return database_type::MONGODB;
    }

    // Check for file extensions (common for SQLite)
    if (connection_string.find(".db") != std::string::npos ||
        connection_string.find(".sqlite") != std::string::npos ||
        connection_string.find(".sqlite3") != std::string::npos ||
        connection_string == ":memory:") {
        return database_type::SQLITE;
    }

    // Default to SQLite for simple file paths
    return database_type::SQLITE;
}

inline bool cdc_factory::is_supported(database_type type) {
    switch (type) {
        case database_type::SQLITE:
            return true;

        case database_type::POSTGRESQL:
        case database_type::MYSQL:
        case database_type::MONGODB:
            // Not yet implemented
            return false;

        default:
            return false;
    }
}

inline std::string cdc_factory::get_type_name(database_type type) {
    switch (type) {
        case database_type::SQLITE:
            return "SQLite";
        case database_type::POSTGRESQL:
            return "PostgreSQL";
        case database_type::MYSQL:
            return "MySQL";
        case database_type::MONGODB:
            return "MongoDB";
        default:
            return "Unknown";
    }
}

} // namespace database::replication::cdc
