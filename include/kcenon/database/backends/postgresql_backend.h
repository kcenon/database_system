// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file postgresql_backend.h
 * @brief PostgreSQL database backend plugin implementation
 *
 * This file implements the database_backend interface for PostgreSQL,
 * directly using libpq/pqxx without depending on the legacy postgres_manager.
 *
 * Issue #286: Update backends to use database_backend only
 * Issue #328: Refactored to use backend_base template
 * - Implements database_backend interface via backend_base CRTP
 * - Registers with backend_registry for runtime selection
 * - Eliminates dependency on database_base-derived classes
 * - Uses Result-based error handling pattern
 */

#pragma once

#include <kcenon/database/core/backend_base.h>
#include <kcenon/database/core/backend_registry.h>

#include <memory>
#include <string>
#include <vector>
#include <atomic>

namespace database
{
namespace backends
{

/**
 * @class postgresql_backend
 * @brief PostgreSQL implementation of database_backend interface
 *
 * This class implements the database_backend interface for PostgreSQL via
 * backend_base CRTP template, using libpq/pqxx libraries.
 *
 * Design Pattern: Strategy pattern with CRTP
 * - Extends backend_base for common lifecycle management
 * - Uses libpq (C API) or pqxx (C++ API) for PostgreSQL access
 * - Provides Result-based error handling
 * - Supports transactions natively
 *
 * Thread Safety:
 * - Thread-safe for read operations (SELECT queries)
 * - Write operations require external synchronization
 *
 * Usage:
 * @code
 *   // Runtime selection via backend_registry
 *   auto backend = backend_registry::instance().create("postgresql");
 *
 *   core::connection_config config;
 *   config.host = "localhost";
 *   config.port = 5432;
 *   config.database = "mydb";
 *   config.username = "user";
 *   config.password = "pass";
 *
 *   if (auto result = backend->initialize(config); !result) {
 *       // Handle error
 *   }
 *
 *   auto rows = backend->select_query("SELECT * FROM users");
 * @endcode
 */
class postgresql_backend
	: public core::backend_base<postgresql_backend, database_types::postgres>
{
public:
	/**
	 * @brief Backend name for error messages
	 */
	static constexpr const char* backend_name() { return "postgresql_backend"; }

	/**
	 * @brief Default constructor
	 */
	postgresql_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~postgresql_backend() override = default;

	// database_backend interface implementation

	kcenon::common::Result<core::database_result> select_query(const std::string& query_string) override;

	kcenon::common::VoidResult execute_query(const std::string& query_string) override;

	[[nodiscard]] kcenon::common::Result<core::database_result> select_prepared(
		const std::string& query,
		const std::vector<core::database_value>& params) override;

	[[nodiscard]] kcenon::common::VoidResult execute_prepared(
		const std::string& query,
		const std::vector<core::database_value>& params) override;

	kcenon::common::VoidResult begin_transaction() override;

	kcenon::common::VoidResult commit_transaction() override;

	kcenon::common::VoidResult rollback_transaction() override;

	bool in_transaction() const override;

	std::string last_error() const override;

	std::map<std::string, std::string> connection_info() const override;

	/**
	 * @brief Execute multiple queries in a single transaction (batch mode).
	 * @param queries Vector of SQL statements to execute atomically.
	 * @return Result containing total affected rows on success, or error on failure.
	 *
	 * All queries run inside a single BEGIN/COMMIT block. If any query fails,
	 * the entire batch is rolled back and the error is returned.
	 */
	kcenon::common::Result<uint64_t> execute_batch(const std::vector<std::string>& queries);

protected:
	friend class core::backend_base<postgresql_backend, database_types::postgres>;

	/**
	 * @brief Database-specific initialization logic
	 * @param config Connection configuration
	 * @return VoidResult::ok() on success, error on failure
	 */
	kcenon::common::VoidResult do_initialize(const core::connection_config& config);

	/**
	 * @brief Database-specific shutdown logic
	 * @return VoidResult::ok() on success, error on failure
	 */
	kcenon::common::VoidResult do_shutdown();

private:
	/**
	 * @brief Convert connection_config to PostgreSQL connection string
	 * @param config Structured connection configuration
	 * @return Connection string for libpq/pqxx
	 *
	 * Format: "host=... port=... dbname=... user=... password=..."
	 */
	std::string build_connection_string(const core::connection_config& config) const;

	/**
	 * @brief Build a connection string with password masked for safe logging
	 * @param config Structured connection configuration
	 * @return Connection string with password replaced by "***"
	 */
	std::string build_safe_connection_string(const core::connection_config& config) const;

	/**
	 * @brief Remove password from an error message that may contain the connection string
	 * @param error_message The error message to sanitize
	 * @return Error message with password masked
	 */
	std::string sanitize_error(const std::string& error_message) const;

	/**
	 * @brief Execute a modification query (INSERT, UPDATE, DELETE)
	 * @param query_string SQL query to execute
	 * @return Number of affected rows
	 */
	unsigned int execute_modification_query(const std::string& query_string);

	void* connection_{nullptr};                 ///< PostgreSQL connection (PGconn* or pqxx::connection*)
	std::atomic<bool> in_transaction_{false};   ///< Transaction state (mirrors active_txn_ under USE_POSTGRESQL)
	void* active_txn_{nullptr};                 ///< Active pqxx::work* when a multi-statement txn is open (USE_POSTGRESQL only; nullptr otherwise)
	mutable std::string last_error_;            ///< Last error message
	core::connection_config connection_config_; ///< Cached connection config
};

} // namespace backends
} // namespace database
