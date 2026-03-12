#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for database_system types
 *
 * This header provides forward declarations for commonly used types
 * in the database_system module to reduce compilation dependencies.
 */

namespace kcenon::database {

/// @name Core classes
/// @{
namespace core {
    /// @brief Top-level database abstraction
    class database;
    /// @brief Dependency injection context for database components
    class database_context;
    /// @brief Database connection handle
    class connection;
    /// @brief ACID transaction scope
    class transaction;
    /// @brief Prepared or ad-hoc SQL statement
    class statement;
    /// @brief Result wrapper for database operations
    template<typename T> class result;
}
/// @}

/// @name Connection management
/// @{
namespace connection {
    /// @brief Factory for creating database connections
    class connection_factory;
    /// @brief Connection configuration parameters
    struct connection_config;
    /// @brief Connection pool usage statistics
    struct connection_stats;
}
/// @}

/// @name Query handling
/// @{
namespace query {
    /// @brief Fluent SQL query builder
    class query_builder;
    /// @brief Query execution engine
    class query_executor;
    /// @brief Pre-compiled parameterized statement
    class prepared_statement;
    /// @brief Batch query executor for bulk operations
    class batch_query;
}
/// @}

/// @name Schema management
/// @{
namespace schema {
    /// @brief Database table representation
    class table;
    /// @brief Table column definition
    class column;
    /// @brief Database index definition
    class index;
    /// @brief Foreign key constraint
    class foreign_key;
    /// @brief Schema migration step
    class migration;
}
/// @}

/// @name ORM support
/// @{
namespace orm {
    /// @brief Object-relational model mapping for type T
    template<typename T> class model;
    /// @brief Object-to-row mapping strategy
    class mapper;
    /// @brief Inter-model relationship definition
    class relation;
    /// @brief Model field validation rules
    class validator;
}
/// @}

/// @name Cache
/// @{
namespace cache {
    /// @brief Cache for compiled query plans
    class query_cache;
    /// @brief Cache for query result sets
    class result_cache;
    /// @brief Cache for idle database connections
    class connection_cache;
}
/// @}

/// @name Async operations
/// @{
namespace async {
    /// @brief Asynchronous result future for type T
    template<typename T> class async_result;
    /// @brief Asynchronous query execution engine
    class async_executor;
    /// @brief Non-blocking database connection
    class async_connection;
}
/// @}

/// @name Utilities
/// @{
namespace utils {
    /// @brief SQL statement parser and validator
    class sql_parser;
    /// @brief Data type conversion between C++ and SQL types
    class data_converter;
    /// @brief Database backup and restore manager
    class backup_manager;
}
/// @}

/// @name Error handling
/// @{
namespace error {
    /// @brief Database operation error with context
    class database_error;
    /// @brief Database-specific error code enumeration
    enum class error_code;
}
/// @}

} // namespace kcenon::database