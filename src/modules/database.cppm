// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file database.cppm
 * @brief Primary C++20 module for database_system.
 *
 * This is the main module interface for the database_system library.
 * It aggregates all module partitions to provide a single import point.
 *
 * Usage:
 * @code
 * import kcenon.database;
 *
 * using namespace database;
 *
 * // Create database context and manager
 * auto context = std::make_shared<database_context>();
 * auto manager = std::make_shared<database_manager>(context);
 *
 * // Configure database type
 * manager->set_mode(database_types::postgres);
 *
 * // Connect and execute queries
 * auto result = manager->connect_result("host=localhost dbname=test");
 * if (result.is_ok()) {
 *     auto query_result = manager->select_query_result("SELECT * FROM users");
 *     // Process results...
 * }
 *
 * // Use query builder
 * auto builder = manager->create_query_builder();
 * auto query = builder
 *     .select({"id", "name"})
 *     .from("users")
 *     .where("active", "=", true)
 *     .limit(10)
 *     .build();
 * @endcode
 *
 * Module Structure:
 * - kcenon.database:core - Core database interfaces and manager
 * - kcenon.database:query - Unified query builder (Strategy pattern)
 * - kcenon.database:backends - Database backend implementations
 *
 * Dependencies:
 * - kcenon.common (Tier 0) - Result<T>, error handling
 * - kcenon.thread (Tier 1) - Thread pool for async operations (optional)
 * - kcenon.container (Tier 1) - Serialization (optional)
 */

export module kcenon.database;

import kcenon.common;

// Tier 1: Core database types and interfaces
export import :core;

// Tier 2: Query building components
export import :query;

// Tier 3: Backend implementations (optional, depends on build configuration)
export import :backends;

export namespace database {

/**
 * @brief Version information for database_system module.
 */
struct module_version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;
    static constexpr int tweak = 0;
    static constexpr const char* string = "0.1.0.0";
    static constexpr const char* module_name = "kcenon.database";
};

} // namespace database
