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
 * @details This file implements the proxy_connector class which handles
 * communication with the database_server middleware. Queries are serialized
 * and sent to the server, which executes them against the actual database.
 */

#include "../database_base.h"
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
 * @details This class implements the database_base interface for proxy mode,
 * where all queries are sent to a database_server middleware instead of
 * directly connecting to the database.
 *
 * ### Current Status (Phase 4.1)
 * This is a stub implementation. The actual network communication will be
 * implemented when database_server is available (Phases 1-3).
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
 * if (connector->connect("")) {
 *     auto result = connector->select_query("SELECT * FROM users");
 *     // Process result...
 *     connector->disconnect();
 * }
 * @endcode
 *
 * @since Phase 4.1
 */
class proxy_connector : public database_base
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

	// Allow move operations
	proxy_connector(proxy_connector&&) noexcept;
	proxy_connector& operator=(proxy_connector&&) noexcept;

	// database_base interface implementation
	database_types database_type() override;
	bool connect(const std::string& connect_string) override;
	bool create_query(const std::string& query_string) override;
	unsigned int insert_query(const std::string& query_string) override;
	unsigned int update_query(const std::string& query_string) override;
	unsigned int delete_query(const std::string& query_string) override;
	database_result select_query(const std::string& query_string) override;
	bool execute_query(const std::string& query_string) override;
	bool disconnect() override;

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
	 * @brief Gets the last error message.
	 * @return Last error message, empty if no error.
	 */
	[[nodiscard]] std::string last_error() const;

	/**
	 * @brief Gets the current configuration.
	 * @return Reference to the proxy connection configuration.
	 */
	[[nodiscard]] const proxy_connection_config& config() const noexcept;

private:
	/**
	 * @brief Attempts to connect to the database_server.
	 * @return true if connection successful, false otherwise.
	 */
	bool try_connect();

	/**
	 * @brief Sends a query to the server and returns the result.
	 *
	 * @param query_type Type of query (SELECT, INSERT, UPDATE, DELETE, etc.)
	 * @param query_string The SQL query string.
	 * @return Response from the server, empty if error.
	 *
	 * @note This is a stub that returns empty/default values until
	 *       database_server is implemented.
	 */
	database_result send_query(const std::string& query_type,
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

	mutable std::mutex mutex_;
	std::string last_error_;
	std::optional<proxy_server_info> server_info_;
};

} // namespace proxy
} // namespace database
