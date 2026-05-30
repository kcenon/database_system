// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <mutex>

// Use unified Result<T> implementation from common_system
#include <kcenon/common/patterns/result.h>
#include <kcenon/database/core/database_context.h>
#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/core/backend_registry.h>

#include <kcenon/database/query_builder.h>


namespace database
{
	/**
	 * @class database_manager
	 * @brief Manages database connections and operations.
	 *
	 * The @c database_manager class provides a high-level interface for
	 * controlling database connections and executing queries. It wraps
	 * a @c database_backend instance and exposes methods such as @c connect,
	 * @c disconnect, @c create_query, @c execute_query, etc.
	 *
	 * @note As of Issue #287, this class uses database_backend internally
	 * instead of the deprecated database_base interface. The public API
	 * remains backward compatible, but also provides new Result-based methods.
	 *
	 * @note As of Sprint 2, this class uses dependency injection pattern.
	 * Access via constructor with database_context parameter.
	 */
	class database_manager
	{
	public:
		/**
		 * @brief Constructor with dependency injection.
		 *
		 * @param context Dependency injection context containing shared components
		 *
		 * @details This constructor accepts a database_context for dependency injection.
		 * Use this for better testability and to support multiple database instances.
		 *
		 * @example
		 * @code
		 * auto context = std::make_shared<database_context>();
		 * auto db_mgr = std::make_shared<database_manager>(context);
		 * @endcode
		 *
		 * @since Sprint 2 (1.0.0)
		 */
		explicit database_manager(std::shared_ptr<database_context> context);

		/**
		 * @brief Destructor.
		 *
		 * Cleans up resources and disconnects from the database if an
		 * active connection exists.
		 */
		virtual ~database_manager();

		/**
		 * @brief Sets the database mode (type) for the manager.
		 *
		 * @param database_type An enum value of @c database_types that
		 *                      specifies the target database type (e.g.,
		 *                      PostgreSQL, SQLite).
		 * @return @c true on success (e.g., if a corresponding database
		 *         implementation is available), @c false otherwise.
		 *
		 */
		bool set_mode(const database_types& database_type);

		/**
		 * @brief Retrieves the current database type used by the manager.
		 *
		 * @return An enum value of @c database_types representing the
		 *         current database mode.
		 */
		database_types database_type(void);


		/**
		 * @brief Creates a query builder for the current database type.
		 *
		 * @return A query builder configured for the current database
		 */
		query_builder create_query_builder();

		/**
		 * @brief Creates a query builder for a specific database type.
		 *
		 * @param db_type The database type to create a builder for
		 * @return A query builder configured for the specified database
		 */
		query_builder create_query_builder(database_types db_type);

		/**
		 * @brief Result-based wrapper for connect().
		 */
		kcenon::common::VoidResult connect_result(const std::string& connect_string);

		/**
		 * @brief Result-based wrapper for disconnect().
		 */
		kcenon::common::VoidResult disconnect_result();

		/**
		 * @brief Result-based wrapper for create_query().
		 */
		kcenon::common::VoidResult create_query_result(const std::string& query_string);

		// Result-based query methods

		/**
		 * @brief Execute a SELECT query and return results.
		 * @param query_string The SQL SELECT statement.
		 * @return Query results, or error.
		 */
		kcenon::common::Result<core::database_result> select_query_result(const std::string& query_string);

		/**
		 * @brief Result-based wrapper for execute_query().
		 * @param query_string The SQL statement.
		 * @return VoidResult indicating success or failure.
		 */
		kcenon::common::VoidResult execute_query_result(const std::string& query_string);

		// Transaction support (new API)

		/**
		 * @brief Begin a database transaction.
		 * @return VoidResult indicating success or failure.
		 */
		kcenon::common::VoidResult begin_transaction();

		/**
		 * @brief Commit the current transaction.
		 * @return VoidResult indicating success or failure.
		 */
		kcenon::common::VoidResult commit_transaction();

		/**
		 * @brief Rollback the current transaction.
		 * @return VoidResult indicating success or failure.
		 */
		kcenon::common::VoidResult rollback_transaction();

		/**
		 * @brief Check if currently in a transaction.
		 * @return true if a transaction is active.
		 */
		bool in_transaction() const;

		/**
		 * @brief Get last error message from the backend.
		 * @return Error message string.
		 */
		std::string last_error() const;

		/**
		 * @brief Get connection information for monitoring.
		 * @return Map of connection properties.
		 */
		std::map<std::string, std::string> connection_info() const;

	private:
		bool connected_; ///< Indicates whether a database connection is active.
		std::unique_ptr<core::database_backend>
			database_;	 ///< The underlying database backend.
		std::shared_ptr<database_context> context_; ///< Dependency injection context
		std::string connect_string_; ///< Cached connection string for initialization

	};
} // namespace kcenon::database

// ---------------------------------------------------------------------------
// Backward-compatibility alias.
// The canonical namespace is `kcenon::database`. The unqualified `database`
// namespace is retained as an alias so existing consumers that reference
// `database::` continue to compile without changes.
// Deprecated spelling: prefer `kcenon::database`. Planned for removal in v1.2.0.
// ---------------------------------------------------------------------------
namespace database = ::kcenon::database;
