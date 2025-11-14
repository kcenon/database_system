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
 * @file postgresql_backend.h
 * @brief PostgreSQL database backend plugin implementation
 *
 * This file implements the database_backend interface for PostgreSQL,
 * adapting the existing postgres_manager implementation to the new
 * plugin architecture.
 *
 * Sprint 5 Task 5.2: Refactor PostgreSQL backend to plugin system
 * - Adapts postgres_manager to database_backend interface
 * - Registers with backend_registry for runtime selection
 * - Maintains compile-time option for convenience (#ifdef USE_POSTGRESQL)
 * - Reduces conditional compilation from scattered usage to single registration point
 */

#pragma once

#include "../core/database_backend.h"
#include "../core/backend_registry.h"
#include "../postgres_manager.h"

#include <memory>
#include <string>
#include <atomic>

namespace database
{
namespace backends
{

/**
 * @class postgresql_backend
 * @brief PostgreSQL implementation of database_backend interface
 *
 * This class adapts the existing postgres_manager to the new database_backend
 * interface, enabling PostgreSQL to work as a plugin in the backend registry.
 *
 * Design Pattern: Adapter pattern
 * - Wraps postgres_manager (existing implementation)
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
class postgresql_backend : public core::database_backend
{
public:
	/**
	 * @brief Default constructor
	 */
	postgresql_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~postgresql_backend() override;

	// Prevent copying (unique ownership of postgres_manager)
	postgresql_backend(const postgresql_backend&) = delete;
	postgresql_backend& operator=(const postgresql_backend&) = delete;

	// Prevent moving (std::atomic members are not moveable)
	postgresql_backend(postgresql_backend&&) noexcept = delete;
	postgresql_backend& operator=(postgresql_backend&&) noexcept = delete;

	/**
	 * @brief Factory method for backend_registry
	 * @return Unique pointer to new postgresql_backend instance
	 */
	static std::unique_ptr<core::database_backend> create();

	// database_backend interface implementation

	database_types type() const override;

	database::result<void> initialize(const core::connection_config& config) override;

	database::result<void> shutdown() override;

	bool is_initialized() const override;

	database::result<uint64_t> insert_query(const std::string& query_string) override;

	database::result<uint64_t> update_query(const std::string& query_string) override;

	database::result<uint64_t> delete_query(const std::string& query_string) override;

	database::result<database_result> select_query(const std::string& query_string) override;

	database::result<void> execute_query(const std::string& query_string) override;

	database::result<void> begin_transaction() override;

	database::result<void> commit_transaction() override;

	database::result<void> rollback_transaction() override;

	bool in_transaction() const override;

	std::string last_error() const override;

	std::map<std::string, std::string> connection_info() const override;

private:
	/**
	 * @brief Convert connection_config to PostgreSQL connection string
	 * @param config Structured connection configuration
	 * @return Connection string for libpq/pqxx
	 *
	 * Format: "host=... port=... dbname=... user=... password=..."
	 */
	std::string build_connection_string(const core::connection_config& config) const;

	std::unique_ptr<postgres_manager> manager_; ///< Underlying PostgreSQL manager
	std::atomic<bool> initialized_{false};      ///< Initialization state
	std::atomic<bool> in_transaction_{false};   ///< Transaction state
	mutable std::string last_error_;            ///< Last error message
	core::connection_config connection_config_; ///< Cached connection config
};

} // namespace backends
} // namespace database
