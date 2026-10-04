// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/core/database_backend.h>

namespace kcenon::database
{
	/**
	 * @class postgres_manager
	 * @brief Manages PostgreSQL database operations.
	 *
	 * This class provides an implementation of the @c database_backend interface
	 * for PostgreSQL databases. It defines methods for connecting, querying,
	 * and disconnecting from a PostgreSQL database.
	 */
	class postgres_manager : public core::database_backend
	{
	public:
		/**
		 * @brief Default constructor.
		 */
		postgres_manager(void);

		/**
		 * @brief Destructor.
		 */
		virtual ~postgres_manager(void);

		/**
		 * @brief Returns the specific type of the database.
		 *
		 * @return An enum value of type @c database_types indicating that
		 *         this is a PostgreSQL database.
		 */
		database_types type() const override;

		/**
		 * @brief Initialize the database backend
		 * @param config Connection configuration
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult initialize(const core::connection_config& config) override;

		/**
		 * @brief Shutdown the database backend gracefully
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult shutdown() override;

		/**
		 * @brief Check if backend is initialized and ready
		 * @return true if initialized and can accept queries
		 */
		bool is_initialized() const override;

		/**
		 * @brief Executes a SELECT query on the connected PostgreSQL database
		 *        and returns the resulting data.
		 *
		 * @param query_string The SQL SELECT query to be executed.
		 * @return Query results as rows, or error
		 */
		kcenon::common::Result<core::database_result> select_query(const std::string& query_string) override;

		/**
		 * @brief Executes a general SQL query (DDL, DML) on PostgreSQL.
		 *
		 * @param query_string The SQL query string to execute.
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult execute_query(const std::string& query_string) override;

		/**
		 * @brief Begin a transaction
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult begin_transaction() override;

		/**
		 * @brief Commit the current transaction
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult commit_transaction() override;

		/**
		 * @brief Rollback the current transaction
		 * @return VoidResult::ok() on success, error on failure
		 */
		kcenon::common::VoidResult rollback_transaction() override;

		/**
		 * @brief Check if backend is currently in a transaction
		 * @return true if transaction is active
		 */
		bool in_transaction() const override;

		/**
		 * @brief Get last error message from backend
		 * @return Error message, or empty string if no error
		 */
		std::string last_error() const override;

		/**
		 * @brief Get backend-specific connection information
		 * @return Map of connection properties (for debugging/monitoring)
		 */
		std::map<std::string, std::string> connection_info() const override;

	private:
		/**
		 * @brief Common implementation for INSERT, UPDATE, and DELETE queries.
		 *
		 * @param query_string The SQL query to be executed.
		 * @return Number of affected rows, or error
		 */
		kcenon::common::Result<uint64_t> execute_modification_query(const std::string& query_string);

	private:
		void* connection_;        ///< Pointer to the underlying PostgreSQL connection object.
		bool initialized_;        ///< Whether the backend is initialized
		bool in_transaction_;     ///< Whether a transaction is active
		std::string last_error_;  ///< Last error message
		std::string connection_string_; ///< Connection string for connection_info
	};
} // namespace kcenon::database
