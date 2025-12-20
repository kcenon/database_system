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
 * @file proxy_config.h
 * @brief Configuration structures for proxy mode connections.
 * @author kcenon
 * @since Phase 4.1
 *
 * @details This file contains configuration structures for connecting
 * to the database_server middleware in proxy mode.
 */

#include <chrono>
#include <string>
#include <cstdint>

namespace database
{
namespace proxy
{

/**
 * @struct proxy_connection_config
 * @brief Configuration for connecting to database_server middleware.
 *
 * @details This structure contains all necessary parameters for establishing
 * a connection to the database_server. Used when connection_mode is set to proxy.
 *
 * @example
 * @code
 * proxy_connection_config config;
 * config.server_host = "db-gateway.internal";
 * config.server_port = 9432;
 * config.auth_token = "client-token-xyz";
 * config.connection_timeout = std::chrono::milliseconds{5000};
 * @endcode
 *
 * @since Phase 4.1
 */
struct proxy_connection_config
{
	/**
	 * @brief Hostname or IP address of the database_server.
	 * @default "localhost"
	 */
	std::string server_host = "localhost";

	/**
	 * @brief Port number of the database_server.
	 * @default 9432
	 */
	uint16_t server_port = 9432;

	/**
	 * @brief Authentication token for connecting to database_server.
	 *
	 * @details This token is used for client authentication when connecting
	 * to the database_server middleware. Obtain this from your administrator.
	 */
	std::string auth_token;

	/**
	 * @brief Timeout for establishing connection to database_server.
	 * @default 5000ms
	 */
	std::chrono::milliseconds connection_timeout{5000};

	/**
	 * @brief Timeout for query execution.
	 * @default 30000ms
	 */
	std::chrono::milliseconds query_timeout{30000};

	/**
	 * @brief Number of retry attempts on connection failure.
	 * @default 3
	 */
	uint8_t retry_count = 3;

	/**
	 * @brief Delay between retry attempts.
	 * @default 1000ms
	 */
	std::chrono::milliseconds retry_delay{1000};

	/**
	 * @brief Enable TLS/SSL for connection to database_server.
	 * @default true
	 */
	bool use_tls = true;

	/**
	 * @brief Path to CA certificate file for TLS verification.
	 *
	 * @details If empty, system CA certificates are used.
	 */
	std::string ca_cert_path;

	/**
	 * @brief Path to client certificate file for mTLS.
	 *
	 * @details Optional. Used for mutual TLS authentication.
	 */
	std::string client_cert_path;

	/**
	 * @brief Path to client private key file for mTLS.
	 *
	 * @details Optional. Used for mutual TLS authentication.
	 */
	std::string client_key_path;

	/**
	 * @brief Validates the configuration.
	 * @return true if configuration is valid, false otherwise.
	 */
	[[nodiscard]] bool is_valid() const noexcept
	{
		return !server_host.empty() &&
			   server_port > 0 &&
			   connection_timeout.count() > 0 &&
			   query_timeout.count() > 0;
	}
};

/**
 * @struct proxy_server_info
 * @brief Information about the connected database_server.
 *
 * @details This structure contains metadata about the database_server
 * that the client is connected to. Useful for debugging and monitoring.
 *
 * @since Phase 4.1
 */
struct proxy_server_info
{
	/**
	 * @brief Server version string.
	 */
	std::string version;

	/**
	 * @brief Server identifier.
	 */
	std::string server_id;

	/**
	 * @brief List of supported database types.
	 */
	std::string supported_databases;

	/**
	 * @brief Maximum connections allowed for this client.
	 */
	uint32_t max_connections = 0;

	/**
	 * @brief Whether the server supports TLS.
	 */
	bool tls_enabled = false;
};

} // namespace proxy
} // namespace database
