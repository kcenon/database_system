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

/**
 * @file proxy_connector.h
 * @brief Database connector for proxy mode communication with database_server.
 * @author kcenon
 * @since Phase 4.1
 *
 * @warning **STUB IMPLEMENTATION** - This is currently a stub. All operations
 *          will return `database_error_code::not_implemented` errors.
 *          Full functionality requires `database_server` (Phases 1-3) which
 *          is not yet available.
 *
 * @details This file implements the proxy_connector class which handles
 * communication with the database_server middleware. Queries are serialized
 * and sent to the server, which executes them against the actual database.
 *
 * @note Updated in Issue #287 to implement database_backend interface instead
 *       of deprecated database_base, enabling database_manager to use
 *       database_backend internally for both direct and proxy modes.
 *
 * @see docs/migration/proxy-mode.md for implementation roadmap and current status.
 */

#include "../core/database_backend.h"
#include "proxy_config.h"

#include <atomic>
#include <mutex>
#include <optional>

namespace database
{
namespace proxy
{

/**
 * @enum proxy_state
 * @brief Connection state for the proxy connector.
 */
enum class proxy_state : uint8_t {
	disconnected = 0,  ///< Not connected to database_server
	connecting = 1,    ///< Connection in progress
	connected = 2,     ///< Connected and ready
	error = 3          ///< Connection error occurred
};

/**
 * @brief Converts proxy_state enum to string representation.
 * @param state The proxy state to convert.
 * @return String representation of the state.
 */
constexpr const char* to_string(proxy_state state) noexcept
{
	switch (state) {
	case proxy_state::disconnected: return "disconnected";
	case proxy_state::connecting: return "connecting";
	case proxy_state::connected: return "connected";
	case proxy_state::error: return "error";
	default: return "unknown";
	}
}

/**
 * @class proxy_connector
 * @brief Database connector for proxy mode operations.
 *
 * @warning **STUB IMPLEMENTATION** - Do not use in production.
 *          All methods will return `not_implemented` errors.
 *
 * @details This class implements the database_backend interface for proxy mode,
 * where all queries are sent to a database_server middleware instead of
 * directly connecting to the database.
 *
 * ### Current Status (Phase 4.1) - STUB
 * This is a **stub implementation**. The actual network communication will be
 * implemented when database_server is available (Phases 1-3).
 *
 * | Operation | Status |
 * |-----------|--------|
 * | initialize() | Returns not_implemented error |
 * | All queries | Returns not_implemented error |
 * | Transactions | Returns not_implemented error |
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Internal state is protected by mutex.
 * - Connection state uses atomic operations.
 *
 * @example
 * @code
 * proxy_connection_config config;
 * config.server_host = "db-gateway.internal";
 * config.server_port = 9432;
 * config.auth_token = "token";
 *
 * auto connector = std::make_unique<proxy_connector>(
 *     database_types::postgres, config);
 *
 * core::connection_config conn_config;
 * if (connector->initialize(conn_config).is_ok()) {
 *     auto result = connector->select_query("SELECT * FROM users");
 *     if (result.is_ok()) {
 *         // Process result...
 *     }
 *     connector->shutdown();
 * }
 * @endcode
 *
 * @since Phase 4.1
 * @note Updated in Issue #287 to implement database_backend instead of database_base
 */
class proxy_connector : public core::database_backend
{
public:
	/**
	 * @brief Constructs a proxy connector.
	 *
	 * @param db_type The target database type on the server side.
	 * @param config Proxy connection configuration.
	 */
	proxy_connector(database_types db_type, const proxy_connection_config& config);

	/**
	 * @brief Destructor - ensures clean disconnection.
	 */
	~proxy_connector() override;

	// Delete copy operations
	proxy_connector(const proxy_connector&) = delete;
	proxy_connector& operator=(const proxy_connector&) = delete;

	// Prevent moving (atomic members are not moveable)
	proxy_connector(proxy_connector&&) noexcept = delete;
	proxy_connector& operator=(proxy_connector&&) noexcept = delete;

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

	/**
	 * @brief Gets the current connection state.
	 * @return Current proxy_state value.
	 */
	[[nodiscard]] proxy_state state() const noexcept;

	/**
	 * @brief Checks if connected to database_server.
	 * @return true if connected, false otherwise.
	 */
	[[nodiscard]] bool is_connected() const noexcept;

	/**
	 * @brief Gets information about the connected server.
	 * @return Server info if connected, nullopt otherwise.
	 */
	[[nodiscard]] std::optional<proxy_server_info> server_info() const;

	/**
	 * @brief Gets the current configuration.
	 * @return Reference to the proxy connection configuration.
	 */
	[[nodiscard]] const proxy_connection_config& config() const noexcept;

private:
	/**
	 * @brief Attempts to connect to the database_server.
	 * @return VoidResult indicating success or failure.
	 */
	kcenon::common::VoidResult try_connect();

	/**
	 * @brief Sends a query to the server and returns the result.
	 *
	 * @param query_type Type of query (SELECT, INSERT, UPDATE, DELETE, etc.)
	 * @param query_string The SQL query string.
	 * @return Response from the server, or error.
	 *
	 * @note This is a stub that returns empty/default values until
	 *       database_server is implemented.
	 */
	kcenon::common::Result<core::database_result> send_query(const std::string& query_type,
							   const std::string& query_string);

	/**
	 * @brief Sets the last error message.
	 * @param message Error message to set.
	 */
	void set_error(const std::string& message);

private:
	database_types db_type_;
	proxy_connection_config config_;
	std::atomic<proxy_state> state_;
	std::atomic<bool> in_transaction_{false};

	mutable std::mutex mutex_;
	mutable std::string last_error_;
	std::optional<proxy_server_info> server_info_;
};

} // namespace proxy
} // namespace database
