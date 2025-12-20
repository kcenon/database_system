// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
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
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file database_types.h
 * @brief Defines the enumeration of supported database types.
 */

#include <cstdint>

namespace database
{
	/**
	 * @enum database_types
	 * @brief Represents various database backends or modes.
	 *
	 * This enumeration is used to specify which database type is being
	 * targeted for operations like connection, querying, and so forth.
	 */
	enum class database_types : uint8_t {
		/**
		 * @brief No specific database type is set.
		 */
		none = 0,

		/**
		 * @brief Indicates a PostgreSQL database.
		 */
		postgres = 1,

		/**
		 * @brief Indicates a MySQL/MariaDB database.
		 */
		mysql = 2,

		/**
		 * @brief Indicates a SQLite database.
		 */
		sqlite = 3,

		/**
		 * @brief Indicates an Oracle database (future implementation).
		 */
		oracle = 4,

		/**
		 * @brief Indicates a MongoDB database (future implementation).
		 */
		mongodb = 5,

		/**
		 * @brief Indicates a Redis database (future implementation).
		 */
		redis = 6
	};

	/**
	 * @enum connection_mode
	 * @brief Represents the connection mode for database operations.
	 *
	 * This enumeration specifies how the client connects to the database:
	 * - direct: Connect directly to the database (legacy mode, uses local pooling)
	 * - proxy: Connect through database_server middleware (recommended for production)
	 *
	 * @since Phase 4.1
	 */
	enum class connection_mode : uint8_t {
		/**
		 * @brief Direct connection to database (legacy mode).
		 *
		 * Uses local connection pooling and direct database driver.
		 * Suitable for simple applications or development environments.
		 */
		direct = 0,

		/**
		 * @brief Connection through database_server middleware.
		 *
		 * Sends queries through database_server for centralized connection
		 * pooling, load balancing, and monitoring. Recommended for production.
		 */
		proxy = 1
	};

	/**
	 * @brief Converts connection_mode enum to string representation.
	 * @param mode The connection mode to convert.
	 * @return String representation of the connection mode.
	 */
	constexpr const char* to_string(connection_mode mode) noexcept
	{
		switch (mode) {
		case connection_mode::direct: return "direct";
		case connection_mode::proxy: return "proxy";
		default: return "unknown";
		}
	}

	/**
	 * @brief Converts database_types enum to string representation.
	 * @param type The database type to convert.
	 * @return String representation of the database type.
	 */
	constexpr const char* to_string(database_types type) noexcept
	{
		switch (type) {
		case database_types::none: return "none";
		case database_types::postgres: return "postgres";
		case database_types::mysql: return "mysql";
		case database_types::sqlite: return "sqlite";
		case database_types::oracle: return "oracle";
		case database_types::mongodb: return "mongodb";
		case database_types::redis: return "redis";
		default: return "unknown";
		}
	}
} // namespace database
