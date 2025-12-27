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
 * @file redis_backend.h
 * @brief Redis database backend plugin implementation
 *
 * This file implements the database_backend interface for Redis,
 * directly using hiredis without depending on the legacy redis_manager.
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
 * @class redis_backend
 * @brief Redis implementation of database_backend interface
 *
 * This class directly implements the database_backend interface for Redis,
 * using hiredis without depending on the legacy redis_manager.
 *
 * Design Pattern: Strategy pattern
 * - Directly implements database_backend interface
 * - Uses hiredis for Redis access
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
 *   auto backend = backend_registry::instance().create("redis");
 *
 *   core::connection_config config;
 *   config.host = "localhost";
 *   config.port = 6379;
 *
 *   if (auto result = backend->initialize(config); !result) {
 *       // Handle error
 *   }
 *
 *   backend->insert_query("key:value");
 *   auto rows = backend->select_query("key");
 * @endcode
 */
class redis_backend : public core::database_backend
{
public:
	/**
	 * @brief Default constructor
	 */
	redis_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~redis_backend() override;

	// Prevent copying (unique ownership of redis_manager)
	redis_backend(const redis_backend&) = delete;
	redis_backend& operator=(const redis_backend&) = delete;

	// Prevent moving (std::atomic members are not moveable)
	redis_backend(redis_backend&&) noexcept = delete;
	redis_backend& operator=(redis_backend&&) noexcept = delete;

	/**
	 * @brief Factory method for backend_registry
	 * @return Unique pointer to new redis_backend instance
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

	kcenon::common::Result<core::database_result> select_query(const std::string& query_string) override;

	kcenon::common::VoidResult execute_query(const std::string& query_string) override;

	kcenon::common::VoidResult begin_transaction() override;

	kcenon::common::VoidResult commit_transaction() override;

	kcenon::common::VoidResult rollback_transaction() override;

	bool in_transaction() const override;

	std::string last_error() const override;

	std::map<std::string, std::string> connection_info() const override;

private:
	/**
	 * @brief Parse Redis query string format
	 * @param query_string Query string in format "key:value"
	 * @param key Output key
	 * @param value Output value
	 * @return true if parsing succeeded
	 */
	bool parse_redis_query(const std::string& query_string,
						   std::string& key,
						   std::string& value) const;

	void* context_{nullptr};                     ///< Redis context (redisContext*)
	std::string host_;                           ///< Redis host
	int port_{6379};                             ///< Redis port
	std::atomic<bool> initialized_{false};       ///< Initialization state
	std::atomic<bool> in_transaction_{false};    ///< Transaction state (MULTI/EXEC)
	mutable std::string last_error_;             ///< Last error message
	core::connection_config connection_config_;  ///< Cached connection config
	mutable std::mutex redis_mutex_;             ///< Mutex for thread safety
};

} // namespace backends
} // namespace database
