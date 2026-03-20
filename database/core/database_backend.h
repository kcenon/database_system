// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file database_backend.h
 * @brief Abstract interface for database backends
 *
 * Defines the interface that all database backends must implement.
 * This enables runtime selection of database implementation without
 * conditional compilation, following the backend pattern successfully
 * implemented in Sprint 4 for logger, monitoring, and thread adapters.
 *
 * Design Goals:
 * - Eliminate conditional compilation (#ifdef USE_POSTGRESQL, etc.)
 * - Enable runtime backend selection
 * - Support plugin architecture for extensibility
 * - Maintain backward compatibility with existing database_base interface
 */

#pragma once

#include "../database_types.h"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace database
{
namespace core
{

// Forward declarations for type aliases
using database_value = std::variant<std::string, int64_t, double, bool, std::nullptr_t>;
using database_row = std::map<std::string, database_value>;
using database_result = std::vector<database_row>;

/**
 * @struct connection_config
 * @brief Configuration for database connection
 *
 * Provides a structured way to pass connection parameters to backends,
 * replacing raw connection strings with typed configuration.
 */
struct connection_config
{
	std::string host;
	uint16_t port = 0;
	std::string database;
	std::string username;
	std::string password;
	std::map<std::string, std::string> options; // Additional driver-specific options

	/**
	 * @brief Construct connection_config from legacy connection string
	 * @param connect_string Connection string (format depends on backend)
	 * @return Parsed connection configuration
	 *
	 * Note: Parsing logic is backend-specific. This is a utility for
	 * backward compatibility.
	 */
	static connection_config from_string(const std::string& connect_string);
};

/**
 * @class database_backend
 * @brief Abstract base class for database backends
 *
 * All database backends (PostgreSQL, SQLite, MongoDB, Redis) must
 * implement this interface. This enables runtime polymorphism and eliminates
 * conditional compilation.
 *
 * Design Pattern:
 * - Strategy pattern: Backends implement different strategies for database access
 * - Factory pattern: backend_registry creates backends by name
 * - RAII: Backends manage connection lifecycle (connect in init, disconnect in shutdown)
 *
 * Thread Safety:
 * - Implementations must be thread-safe for read operations
 * - Write operations require external synchronization or internal locking
 *
 * Example Usage:
 * @code
 *   auto backend = backend_registry::create("postgresql", config);
 *   if (auto result = backend->initialize(); !result) {
 *       // Handle initialization error
 *   }
 *   auto rows = backend->select_query("SELECT * FROM users");
 *   backend->execute_query("INSERT INTO users (name) VALUES ('John')");
 *   backend->shutdown();
 * @endcode
 */
class database_backend
{
public:
	virtual ~database_backend() = default;

	/**
	 * @brief Get the database type of this backend
	 * @return Database type identifier
	 */
	virtual database_types type() const = 0;

	/**
	 * @brief Initialize the database backend
	 * @param config Connection configuration
	 * @return VoidResult::ok() on success, error on failure
	 *
	 * This method should:
	 * - Establish database connection
	 * - Validate connection parameters
	 * - Set up connection pooling if applicable
	 * - Initialize backend-specific resources
	 */
	virtual kcenon::common::VoidResult initialize(const connection_config& config) = 0;

	/**
	 * @brief Shutdown the database backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 *
	 * This method should:
	 * - Close active connections
	 * - Release connection pool resources
	 * - Flush pending transactions
	 * - Clean up backend-specific resources
	 */
	virtual kcenon::common::VoidResult shutdown() = 0;

	/**
	 * @brief Check if backend is initialized and ready
	 * @return true if initialized and can accept queries
	 */
	virtual bool is_initialized() const = 0;

	/**
	 * @brief Execute a SELECT query
	 * @param query_string SQL SELECT statement
	 * @return Query results as rows, or error
	 */
	virtual kcenon::common::Result<database_result> select_query(const std::string& query_string) = 0;

	/**
	 * @brief Execute a general SQL query (DDL, DML)
	 * @param query_string SQL statement
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual kcenon::common::VoidResult execute_query(const std::string& query_string) = 0;

	/**
	 * @brief Begin a transaction
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual kcenon::common::VoidResult begin_transaction() = 0;

	/**
	 * @brief Commit the current transaction
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual kcenon::common::VoidResult commit_transaction() = 0;

	/**
	 * @brief Rollback the current transaction
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual kcenon::common::VoidResult rollback_transaction() = 0;

	/**
	 * @brief Check if backend is currently in a transaction
	 * @return true if transaction is active
	 */
	virtual bool in_transaction() const = 0;

	/**
	 * @brief Get last error message from backend
	 * @return Error message, or empty string if no error
	 */
	virtual std::string last_error() const = 0;

	/**
	 * @brief Get backend-specific connection information
	 * @return Map of connection properties (for debugging/monitoring)
	 *
	 * Example keys: "server_version", "connection_id", "protocol_version"
	 */
	virtual std::map<std::string, std::string> connection_info() const = 0;
};

/**
 * @brief Factory function type for creating database backends
 *
 * This function signature is used by backend_registry to instantiate backends.
 * Each backend implementation should provide a create() function matching this signature.
 *
 * @return Unique pointer to newly created backend instance
 */
using backend_factory_fn = std::unique_ptr<database_backend> (*)();

} // namespace core
} // namespace database
