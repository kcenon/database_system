#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for database_system types
 *
 * This header provides forward declarations for commonly used types
 * in the database_system module to reduce compilation dependencies.
 */

namespace kcenon::database {

// Core classes
namespace core {
    class database;
    class database_context;
    class connection;
    class transaction;
    class statement;
    template<typename T> class result;
}

// Connection management
namespace connection {
    class connection_pool;
    class connection_factory;
    struct connection_config;
    struct connection_stats;
}

// Query handling
namespace query {
    class query_builder;
    class query_executor;
    class prepared_statement;
    class batch_query;
}

// Schema management
namespace schema {
    class table;
    class column;
    class index;
    class foreign_key;
    class migration;
}

// ORM support
namespace orm {
    template<typename T> class model;
    class mapper;
    class relation;
    class validator;
}

// Cache
namespace cache {
    class query_cache;
    class result_cache;
    class connection_cache;
}

// Async operations
namespace async {
    template<typename T> class async_result;
    class async_executor;
    class async_connection;
}

// Server
namespace server {
    class database_proxy_server;
    class request_handler;
    class session_manager;
}

// Utilities
namespace utils {
    class sql_parser;
    class data_converter;
    class backup_manager;
}

// Error handling
namespace error {
    class database_error;
    enum class error_code;
}

} // namespace kcenon::database