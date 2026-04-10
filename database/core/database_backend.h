// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
	 * @brief Execute a parameterized SELECT query (prepared statement)
	 *
	 * Parameters are bound at the wire-protocol level, providing stronger
	 * SQL injection protection than string escaping. Backends that support
	 * native prepared statements (PostgreSQL, SQLite) should override this.
	 *
	 * @param query SQL with positional placeholders ($1, $2, ... or ?, ?, ...)
	 * @param params Parameter values to bind
	 * @return Query results as rows, or error
	 *
	 * @note Default implementation falls back to string interpolation via
	 *       execute_query/select_query for backends that have not yet
	 *       implemented native prepared statement support.
	 */
	[[nodiscard]] virtual kcenon::common::Result<database_result> select_prepared(
		const std::string& query,
		const std::vector<database_value>& params)
	{
		// Default fallback: substitute params inline (less secure, but functional)
		auto expanded = expand_params(query, params);
		return select_query(expanded);
	}

	/**
	 * @brief Execute a parameterized DML/DDL query (prepared statement)
	 *
	 * @param query SQL with positional placeholders ($1, $2, ... or ?, ?, ...)
	 * @param params Parameter values to bind
	 * @return VoidResult::ok() on success, error on failure
	 *
	 * @see select_prepared for details on parameterized queries
	 */
	[[nodiscard]] virtual kcenon::common::VoidResult execute_prepared(
		const std::string& query,
		const std::vector<database_value>& params)
	{
		auto expanded = expand_params(query, params);
		return execute_query(expanded);
	}

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

protected:
	/**
	 * @brief Expand positional parameters into a SQL string (fallback)
	 *
	 * Substitutes $1, $2, ... or ?, ?, ... placeholders with stringified
	 * parameter values. Used by the default select_prepared/execute_prepared
	 * implementations. Backends with native prepared statements should
	 * override the virtual methods instead of relying on this.
	 *
	 * @warning This performs string interpolation, NOT wire-level binding.
	 *          Override select_prepared/execute_prepared for true security.
	 */
	static std::string expand_params(
		const std::string& query,
		const std::vector<database_value>& params)
	{
		std::string result = query;

		// Replace $N placeholders (PostgreSQL-style, 1-indexed)
		for (size_t i = params.size(); i > 0; --i) {
			auto placeholder = "$" + std::to_string(i);
			auto pos = result.find(placeholder);
			if (pos != std::string::npos) {
				result.replace(pos, placeholder.size(), value_to_sql(params[i - 1]));
			}
		}

		// Replace ? placeholders (SQLite-style, left-to-right)
		size_t param_idx = 0;
		auto pos = result.find('?');
		while (pos != std::string::npos && param_idx < params.size()) {
			auto val = value_to_sql(params[param_idx++]);
			result.replace(pos, 1, val);
			pos = result.find('?', pos + val.size());
		}

		return result;
	}

private:
	static std::string value_to_sql(const database_value& val)
	{
		return std::visit([](const auto& v) -> std::string {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::nullptr_t>) {
				return "NULL";
			} else if constexpr (std::is_same_v<T, bool>) {
				return v ? "TRUE" : "FALSE";
			} else if constexpr (std::is_same_v<T, std::string>) {
				// Basic escaping — backends should override for proper security
				std::string escaped;
				escaped.reserve(v.size() + 2);
				escaped += '\'';
				for (char c : v) {
					if (c == '\'') escaped += "''";
					else escaped += c;
				}
				escaped += '\'';
				return escaped;
			} else {
				return std::to_string(v);
			}
		}, val);
	}
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
