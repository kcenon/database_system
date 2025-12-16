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
 * @file sqlite_backend.h
 * @brief SQLite database backend plugin implementation
 *
 * This file implements the database_backend interface for SQLite,
 * adapting the existing sqlite_manager implementation to the new
 * plugin architecture.
 *
 * Sprint 5 Task 5.3: Refactor SQLite backend to plugin system
 * - Adapts sqlite_manager to database_backend interface
 * - Registers with backend_registry for runtime selection
 * - Maintains compile-time option for convenience (#ifdef USE_SQLITE)
 * - Reduces conditional compilation from scattered usage to single registration point
 */

#pragma once

#include "../core/database_backend.h"
#include "../core/backend_registry.h"
#include "sqlite/sqlite_manager.h"

#include <memory>
#include <string>
#include <atomic>

namespace database
{
namespace backends
{

/**
 * @class sqlite_backend
 * @brief SQLite implementation of database_backend interface
 *
 * This class adapts the existing sqlite_manager to the new database_backend
 * interface, enabling SQLite to work as a plugin in the backend registry.
 *
 * Design Pattern: Adapter pattern
 * - Wraps sqlite_manager (existing implementation)
 * - Adapts database_base interface to database_backend interface
 * - Converts return types (bool/unsigned int → Result<T>)
 * - Converts connection params (string → connection_config)
 *
 * Thread Safety:
 * - Thread-safe for read operations (SELECT queries)
 * - Write operations require external synchronization
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
class sqlite_backend : public core::database_backend
{
public:
	/**
	 * @brief Default constructor
	 */
	sqlite_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~sqlite_backend() override;

	// Prevent copying (unique ownership of sqlite_manager)
	sqlite_backend(const sqlite_backend&) = delete;
	sqlite_backend& operator=(const sqlite_backend&) = delete;

	// Prevent moving (std::atomic members are not moveable)
	sqlite_backend(sqlite_backend&&) noexcept = delete;
	sqlite_backend& operator=(sqlite_backend&&) noexcept = delete;

	/**
	 * @brief Factory method for backend_registry
	 * @return Unique pointer to new sqlite_backend instance
	 */
	static std::unique_ptr<core::database_backend> create();

	// database_backend interface implementation

	database_types type() const override;

	kcenon::common::VoidResult initialize(const core::connection_config& config) override;

	kcenon::common::VoidResult shutdown() override;

	bool is_initialized() const override;

	kcenon::common::Result<uint64_t> insert_query(const std::string& query_string) override;

	kcenon::common::Result<uint64_t> update_query(const std::string& query_string) override;

	kcenon::common::Result<uint64_t> delete_query(const std::string& query_string) override;

	kcenon::common::Result<database_result> select_query(const std::string& query_string) override;

	kcenon::common::VoidResult execute_query(const std::string& query_string) override;

	kcenon::common::VoidResult begin_transaction() override;

	kcenon::common::VoidResult commit_transaction() override;

	kcenon::common::VoidResult rollback_transaction() override;

	bool in_transaction() const override;

	std::string last_error() const override;

	std::map<std::string, std::string> connection_info() const override;

private:
	/**
	 * @brief Extract database file path from connection_config
	 * @param config Structured connection configuration
	 * @return Database file path or ":memory:" for in-memory database
	 *
	 * SQLite uses file paths instead of network connections.
	 * The database field contains the file path.
	 */
	std::string get_database_path(const core::connection_config& config) const;

	std::unique_ptr<sqlite_manager> manager_; ///< Underlying SQLite manager
	std::atomic<bool> initialized_{false};    ///< Initialization state
	std::atomic<bool> in_transaction_{false}; ///< Transaction state
	mutable std::string last_error_;          ///< Last error message
	core::connection_config connection_config_; ///< Cached connection config
};

} // namespace backends
} // namespace database
