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
 * @file mongodb_backend.h
 * @brief MongoDB database backend plugin implementation
 *
 * This file implements the database_backend interface for MongoDB,
 * directly using the MongoDB C++ driver without depending on the legacy mongodb_manager.
 *
 * Issue #286: Update backends to use database_backend only
 * - Implements database_backend interface directly
 * - Registers with backend_registry for runtime selection
 * - Eliminates dependency on database_base-derived classes
 * - Uses Result-based error handling pattern
 */

#pragma once

#include "../core/database_backend.h"
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
 * @class mongodb_backend
 * @brief MongoDB implementation of database_backend interface
 *
 * This class directly implements the database_backend interface for MongoDB,
 * using the MongoDB C++ driver without depending on the legacy mongodb_manager.
 *
 * Design Pattern: Strategy pattern
 * - Directly implements database_backend interface
 * - Uses mongocxx driver for database access
 * - Provides Result-based error handling
 * - Thread-safe with internal mutex
 *
 * Thread Safety:
 * - All operations are thread-safe via mutex
 * - Suitable for multi-threaded access
 *
 * Usage:
 * @code
 *   // Runtime selection via backend_registry
 *   auto backend = backend_registry::instance().create("mongodb");
 *
 *   core::connection_config config;
 *   config.host = "localhost";
 *   config.port = 27017;
 *   config.database = "mydb";
 *   config.username = "user";
 *   config.password = "pass";
 *
 *   if (auto result = backend->initialize(config); !result) {
 *       // Handle error
 *   }
 *
 *   auto rows = backend->select_query("users:{\"name\":\"John\"}");
 * @endcode
 */
class mongodb_backend : public core::database_backend
{
public:
	/**
	 * @brief Default constructor
	 */
	mongodb_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~mongodb_backend() override;

	// Prevent copying (unique ownership of mongodb_manager)
	mongodb_backend(const mongodb_backend&) = delete;
	mongodb_backend& operator=(const mongodb_backend&) = delete;

	// Prevent moving (std::atomic members are not moveable)
	mongodb_backend(mongodb_backend&&) noexcept = delete;
	mongodb_backend& operator=(mongodb_backend&&) noexcept = delete;

	/**
	 * @brief Factory method for backend_registry
	 * @return Unique pointer to new mongodb_backend instance
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
	 * @brief Convert connection_config to MongoDB connection URI
	 * @param config Structured connection configuration
	 * @return Connection URI for MongoDB C++ driver
	 *
	 * Format: "mongodb://username:password@host:port/database"
	 */
	std::string build_connection_uri(const core::connection_config& config) const;

	/**
	 * @brief Parse query string format
	 * @param query_string Query string in format "collection:filter:update"
	 * @param collection Output collection name
	 * @param filter Output filter JSON
	 * @param update Output update JSON (optional)
	 * @return true if parsing succeeded
	 */
	bool parse_query_string(const std::string& query_string,
							std::string& collection,
							std::string& filter,
							std::string& update) const;

	void* client_{nullptr};                      ///< MongoDB client (mongocxx::client*)
	void* database_{nullptr};                    ///< MongoDB database (mongocxx::database*)
	std::string db_name_;                        ///< Database name
	std::atomic<bool> initialized_{false};       ///< Initialization state
	std::atomic<bool> in_transaction_{false};    ///< Transaction state
	mutable std::string last_error_;             ///< Last error message
	core::connection_config connection_config_;  ///< Cached connection config
	mutable std::mutex mongo_mutex_;             ///< Mutex for thread safety
};

} // namespace backends
} // namespace database
