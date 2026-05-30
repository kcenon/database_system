// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file redis_backend.h
 * @brief Redis database backend plugin implementation
 *
 * This file implements the database_backend interface for Redis,
 * directly using hiredis without depending on the legacy redis_manager.
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
 * This class implements the database_backend interface for Redis via
 * backend_base CRTP template, using hiredis.
 *
 * Design Pattern: Strategy pattern with CRTP
 * - Extends backend_base for common lifecycle management
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
 *   backend->execute_query("SET key value");
 *   auto rows = backend->select_query("key");
 * @endcode
 */
class redis_backend
	: public core::backend_base<redis_backend, database_types::redis>
{
public:
	/**
	 * @brief Backend name for error messages
	 */
	static constexpr const char* backend_name() { return "redis_backend"; }

	/**
	 * @brief Default constructor
	 */
	redis_backend();

	/**
	 * @brief Destructor - ensures proper cleanup
	 */
	~redis_backend() override = default;

	// database_backend interface implementation

	kcenon::common::Result<core::database_result> select_query(const std::string& query_string) override;

	kcenon::common::VoidResult execute_query(const std::string& query_string) override;

	kcenon::common::VoidResult begin_transaction() override;

	kcenon::common::VoidResult commit_transaction() override;

	kcenon::common::VoidResult rollback_transaction() override;

	bool in_transaction() const override;

	std::string last_error() const override;

	std::map<std::string, std::string> connection_info() const override;

protected:
	friend class core::backend_base<redis_backend, database_types::redis>;

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
	std::atomic<bool> in_transaction_{false};    ///< Transaction state (MULTI/EXEC)
	mutable std::string last_error_;             ///< Last error message
	core::connection_config connection_config_;  ///< Cached connection config
	mutable std::mutex redis_mutex_;             ///< Mutex for thread safety
};

} // namespace backends
} // namespace kcenon::database
