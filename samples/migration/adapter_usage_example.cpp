/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * database_base_adapter Usage Example
 *
 * This example demonstrates how to use the database_base_adapter class
 * to bridge legacy code using database_base with the new database_backend
 * interface.
 *
 * The adapter pattern allows gradual migration from the deprecated
 * database_base interface to the modern database_backend interface.
 *
 * Migration Path:
 * 1. Legacy code continues to work with database_base_adapter
 * 2. New code uses database_backend directly
 * 3. Gradually migrate legacy code to database_backend
 * 4. Remove adapter usage when migration is complete
 *
 * @see docs/MIGRATION_database_base.md for complete migration guide
 */

#include <iostream>
#include <memory>
#include <string>

// Include the adapter header
#include "database/database_base_adapter.h"
#include "database/core/backend_registry.h"

// Suppress deprecation warnings since we're demonstrating legacy usage
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

using namespace database;

//=============================================================================
// Legacy code that expects database_base interface
//=============================================================================

/**
 * @brief Example legacy function that uses database_base
 *
 * This represents existing code that was written before database_backend.
 * Using the adapter, this code continues to work without modification.
 */
void legacy_query_function(database_base& db) {
    std::cout << "Legacy function executing query...\n";

    // Legacy interface uses bool returns
    bool success = db.execute_query("CREATE TABLE IF NOT EXISTS users ("
                                     "id INTEGER PRIMARY KEY, "
                                     "name TEXT, "
                                     "email TEXT)");

    if (success) {
        std::cout << "Table created successfully\n";
    }

    // Legacy interface returns unsigned int for row count
    unsigned int inserted = db.insert_query(
        "INSERT INTO users (id, name, email) VALUES "
        "(1, 'Alice', 'alice@example.com')");

    std::cout << "Inserted " << inserted << " row(s)\n";

    // Legacy interface returns database_result directly
    auto result = db.select_query("SELECT * FROM users");
    std::cout << "Selected " << result.size() << " row(s)\n";

    for (const auto& row : result) {
        for (const auto& [key, value] : row) {
            std::cout << "  " << key << ": ";
            std::visit([](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << v;
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    std::cout << v;
                } else if constexpr (std::is_same_v<T, double>) {
                    std::cout << v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    std::cout << (v ? "true" : "false");
                } else {
                    std::cout << "null";
                }
            }, value);
            std::cout << "\n";
        }
    }
}

//=============================================================================
// Modern code using database_backend directly
//=============================================================================

/**
 * @brief Example modern function that uses database_backend
 *
 * This represents new code that takes advantage of Result<T> error handling.
 */
void modern_query_function(core::database_backend& backend) {
    std::cout << "\nModern function executing query...\n";

    // Modern interface uses Result<T> for proper error handling
    auto exec_result = backend.execute_query("CREATE TABLE IF NOT EXISTS products ("
                                              "id INTEGER PRIMARY KEY, "
                                              "name TEXT, "
                                              "price REAL)");

    if (exec_result.is_ok()) {
        std::cout << "Table created successfully\n";
    } else {
        std::cout << "Error: " << exec_result.error() << "\n";
        return;
    }

    // Modern interface returns Result<uint64_t> for row count
    auto insert_result = backend.insert_query(
        "INSERT INTO products (id, name, price) VALUES "
        "(1, 'Widget', 19.99)");

    if (insert_result.is_ok()) {
        std::cout << "Inserted " << insert_result.value() << " row(s)\n";
    } else {
        std::cout << "Insert error: " << insert_result.error() << "\n";
    }

    // Modern interface returns Result<database_result>
    auto select_result = backend.select_query("SELECT * FROM products");

    if (select_result.is_ok()) {
        std::cout << "Selected " << select_result.value().size() << " row(s)\n";
        for (const auto& row : select_result.value()) {
            for (const auto& [key, value] : row) {
                std::cout << "  " << key << ": ";
                std::visit([](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        std::cout << v;
                    } else if constexpr (std::is_same_v<T, int64_t>) {
                        std::cout << v;
                    } else if constexpr (std::is_same_v<T, double>) {
                        std::cout << v;
                    } else if constexpr (std::is_same_v<T, bool>) {
                        std::cout << (v ? "true" : "false");
                    } else {
                        std::cout << "null";
                    }
                }, value);
                std::cout << "\n";
            }
        }
    } else {
        std::cout << "Select error: " << select_result.error() << "\n";
    }
}

//=============================================================================
// Main demonstration
//=============================================================================

int main() {
    std::cout << "=== database_base_adapter Usage Example ===\n\n";

#ifdef USE_SQLITE
    // Step 1: Create a modern database_backend using the registry
    std::cout << "Creating SQLite backend...\n";
    auto backend = core::backend_registry::instance().create("sqlite");

    if (!backend) {
        std::cerr << "Failed to create SQLite backend\n";
        return 1;
    }

    // Step 2: Initialize the backend
    core::connection_config config;
    config.database = ":memory:";  // Use in-memory database for demo

    auto init_result = backend->initialize(config);
    if (!init_result.is_ok()) {
        std::cerr << "Failed to initialize: " << init_result.error() << "\n";
        return 1;
    }

    // Step 3: Demonstrate modern usage directly
    std::cout << "\n--- Using database_backend directly ---\n";
    modern_query_function(*backend);

    // Step 4: Create adapter for legacy code
    std::cout << "\n--- Using database_base_adapter for legacy code ---\n";

    // Note: The adapter takes ownership of the backend.
    // For demo purposes, we create a new backend for the adapter.
    auto legacy_backend = core::backend_registry::instance().create("sqlite");
    auto adapter = std::make_unique<database_base_adapter>(std::move(legacy_backend));

    // Connect using the legacy interface (adapter parses connection string)
    if (!adapter->connect(":memory:")) {
        std::cerr << "Adapter connection failed\n";
        return 1;
    }

    // Call legacy function - it works seamlessly with the adapter
    legacy_query_function(*adapter);

    // Clean up
    adapter->disconnect();

    std::cout << "\n--- Migration Benefits ---\n";
    std::cout << "1. Legacy code continues to work with adapter\n";
    std::cout << "2. New code uses Result<T> for better error handling\n";
    std::cout << "3. Both can coexist during gradual migration\n";
    std::cout << "4. When migration is complete, remove adapter usage\n";

    backend->shutdown();
#else
    std::cout << "SQLite not available. This example requires USE_SQLITE.\n";
    std::cout << "Build with: cmake -DUSE_SQLITE=ON ..\n";
#endif

    return 0;
}

// Restore diagnostic settings
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
