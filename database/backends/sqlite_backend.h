// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file sqlite_backend.h
 * @brief SQLite database backend plugin implementation
 *
 * This file implements the database_backend interface for SQLite,
 * directly using the SQLite3 C API without depending on the legacy sqlite_manager.
 *
 * Issue #286: Update backends to use database_backend only
 * Issue #328: Refactored to use backend_base template
 * - Implements database_backend interface via backend_base CRTP
 * - Registers with backend_registry for runtime selection
 * - Eliminates dependency on database_base-derived classes
 * - Uses Result-based error handling pattern
 */

#pragma once

#include "../core/backend_base.h"
#include "../core/backend_registry.h"

#include <memory>
#include <string>
#include <atomic>
#include <mutex>

namespace database
{
namespace backends
{

/**
 * @class sqlite_backend
 * @brief SQLite implementation of database_backend interface
 *
 * This class implements the database_backend interface for SQLite via
 * backend_base CRTP template, using the SQLite3 C API.
 *
 * Design Pattern: Strategy pattern with CRTP
 * - Extends backend_base for common lifecycle management
 * - Uses SQLite3 C API for database access
 * - Provides Result-based error handling
 * - Supports transactions natively
 * - Thread-safe with internal mutex
 *
 * Thread Safety:
 * - All operations are thread-safe via recursive mutex
 * - Suitable for multi-threaded access
 *
 * Usage:
 * @code
 *   // Runtime selection via backend_registry
 *   auto backend = backend_registry::instance().create("sqlite");
 *
 *   core::connection_config config;
 *   config.database = "./mydb.db";  // File path or ":memory:" for in-memory DB
 *
 *   if (auto result = backend->initialize(config); !result) {
 *       // Handle error
 *   }
 *
 *   auto rows = backend->select_query("SELECT * FROM users");
 * @endcode
 */
class sqlite_backend
	: public core::backend_base<sqlite_backend, database_types::sqlite>
{
public:
	/**
	 * @brief Backend name for error messages
	 */
	static constexpr const char* backend_name() { return "sqlite_backend"; }

	/**
	 * @brief Default constructor
	 */
	sqlite_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~sqlite_backend() override = default;

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

protected:
	friend class core::backend_base<sqlite_backend, database_types::sqlite>;

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
	 * @brief Execute a modification query (INSERT, UPDATE, DELETE)
	 * @param query_string SQL query to execute
	 * @return Number of affected rows
	 */
	unsigned int execute_modification_query(const std::string& query_string);

	/**
	 * @brief Convert SQLite column value to database_value
	 * @param stmt SQLite prepared statement
	 * @param column_index Column index in the result set
	 * @return database_value containing the converted value
	 */
	core::database_value convert_sqlite_value(void* stmt, int column_index);

	void* connection_{nullptr};                      ///< SQLite connection (sqlite3*)
	std::atomic<bool> in_transaction_{false};        ///< Transaction state
	mutable std::string last_error_;                 ///< Last error message
	core::connection_config connection_config_;      ///< Cached connection config
	mutable std::recursive_mutex sqlite_mutex_;      ///< Mutex for thread safety
};

} // namespace backends
} // namespace database
