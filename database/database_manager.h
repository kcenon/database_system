/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

#include <memory>
#include <mutex>

// Use unified Result<T> implementation from common_system
#include <kcenon/common/patterns/result.h>
#include "core/database_context.h"
#include "core/database_backend.h"
#include "core/backend_registry.h"

#include "query_builder.h"
#include "proxy/proxy_config.h"


namespace database
{
	/**
	 * @class database_manager
	 * @brief Manages database connections and operations.
	 *
	 * The @c database_manager class provides a high-level interface for
	 * controlling database connections and executing queries. It wraps
	 * a @c database_backend instance and exposes methods such as @c connect,
	 * @c disconnect, @c create_query, @c insert_query, etc.
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
		 *                      PostgreSQL, MySQL, SQLite).
		 * @return @c true on success (e.g., if a corresponding database
		 *         implementation is available), @c false otherwise.
		 *
		 * @note This uses DirectMode (direct database connection) by default.
		 *       Use set_mode_proxy() for ProxyMode connections.
		 */
		bool set_mode(const database_types& database_type);

		/**
		 * @brief Sets the database mode for proxy connection.
		 *
		 * @param database_type The target database type on the server side.
		 * @param proxy_config Configuration for connecting to database_server.
		 * @return @c true on success, @c false otherwise.
		 *
		 * @details In proxy mode, all queries are sent to the database_server
		 * middleware instead of directly connecting to the database. This provides:
		 * - Centralized connection pooling
		 * - Load balancing
		 * - Unified monitoring
		 * - Secure credential management
		 *
		 * @example
		 * @code
		 * auto db_mgr = std::make_shared<database_manager>(context);
		 * proxy::proxy_connection_config config;
		 * config.server_host = "db-gateway.internal";
		 * config.server_port = 9432;
		 * config.auth_token = "token";
		 * if (db_mgr->set_mode_proxy(database_types::postgres, config)) {
		 *     db_mgr->connect(""); // connect_string ignored in proxy mode
		 * }
		 * @endcode
		 *
		 * @since Phase 4.1
		 */
		bool set_mode_proxy(const database_types& database_type,
							const proxy::proxy_connection_config& proxy_config);

		/**
		 * @brief Gets the current connection mode.
		 * @return The current connection_mode (direct or proxy).
		 * @since Phase 4.1
		 */
		connection_mode current_connection_mode() const noexcept;

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

		// Result-based query methods (new API)

		/**
		 * @brief Result-based wrapper for insert_query().
		 * @param query_string The SQL INSERT statement.
		 * @return Number of rows inserted, or error.
		 */
		kcenon::common::Result<uint64_t> insert_query_result(const std::string& query_string);

		/**
		 * @brief Result-based wrapper for update_query().
		 * @param query_string The SQL UPDATE statement.
		 * @return Number of rows updated, or error.
		 */
		kcenon::common::Result<uint64_t> update_query_result(const std::string& query_string);

		/**
		 * @brief Result-based wrapper for delete_query().
		 * @param query_string The SQL DELETE statement.
		 * @return Number of rows deleted, or error.
		 */
		kcenon::common::Result<uint64_t> delete_query_result(const std::string& query_string);

		/**
		 * @brief Result-based wrapper for select_query().
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
		connection_mode connection_mode_; ///< Current connection mode (Phase 4.1)
		proxy::proxy_connection_config proxy_config_; ///< Proxy configuration (Phase 4.1)
		std::string connect_string_; ///< Cached connection string for initialization

	};
} // namespace database
