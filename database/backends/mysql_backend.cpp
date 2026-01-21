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

#include "mysql_backend.h"
#include "../core/result.h"

#ifdef USE_MYSQL
#include <mysql.h>
#endif

#include <sstream>
#include <iomanip>
#include <variant>
#include <iostream>

#include "../utils/backend_logger.h"

namespace
{
const database::utils::backend_logger logger_("MySQL");
}

namespace database
{
namespace backends
{

mysql_backend::mysql_backend()
	: connection_(nullptr)
{
}

kcenon::common::VoidResult mysql_backend::do_initialize(const core::connection_config& config)
{
	connection_config_ = config;

#ifdef USE_MYSQL
	try {
		// Initialize MySQL connection
		MYSQL* mysql = mysql_init(nullptr);
		if (!mysql) {
			last_error_ = "MySQL library initialization failed";
			logger_.error("do_initialize", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"mysql_backend"
			};
		}

		// Attempt to connect
		const char* host = config.host.empty() ? "localhost" : config.host.c_str();
		unsigned int port = config.port > 0 ? config.port : 3306;

		connection_ = mysql_real_connect(
			mysql,
			host,
			config.username.c_str(),
			config.password.c_str(),
			config.database.c_str(),
			port,
			nullptr,
			0
		);

		if (!connection_) {
			last_error_ = std::string("Connection failed: ") + mysql_error(mysql);
			logger_.error("do_initialize", last_error_);
			mysql_close(mysql);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"mysql_backend"
			};
		}

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		logger_.error("do_initialize", last_error_);
	}
#else
	logger_.warning("MySQL support not compiled. Mock mode enabled.");
	// Mock mode for testing without MySQL
	last_error_.clear();
	return kcenon::common::ok();
#endif

	if (last_error_.empty()) {
		last_error_ = "Failed to connect to MySQL server";
	}
	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"mysql_backend"
	};
}

kcenon::common::VoidResult mysql_backend::do_shutdown()
{
	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_MYSQL
	if (connection_) {
		MYSQL* mysql = static_cast<MYSQL*>(connection_);
		mysql_close(mysql);
		connection_ = nullptr;
	}
#endif

	last_error_.clear();
	return kcenon::common::ok();
}

unsigned int mysql_backend::execute_modification_query(const std::string& query_string)
{
#ifdef USE_MYSQL
	if (!connection_) return 0;
	try {
		MYSQL* mysql = static_cast<MYSQL*>(connection_);
		if (mysql_query(mysql, query_string.c_str()) != 0) {
			last_error_ = std::string("Modification query failed: ") + mysql_error(mysql);
			logger_.error("execute_modification_query", last_error_);
			return 0;
		}
		last_error_.clear();
		return static_cast<unsigned int>(mysql_affected_rows(mysql));
	} catch (const std::exception& e) {
		last_error_ = std::string("Modification query error: ") + e.what();
		logger_.error("execute_modification_query", last_error_);
	}
#else
	logger_.warning("MySQL support not compiled. Modification query: " + query_string.substr(0, 20) + "...");
	return 1; // Mock: return 1 affected row
#endif
	return 0;
}

kcenon::common::Result<uint64_t> mysql_backend::insert_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> mysql_backend::update_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> mysql_backend::delete_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<core::database_result> mysql_backend::select_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	core::database_result result;

#ifdef USE_MYSQL
	if (!connection_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mysql_backend"
		};
	}
	try {
		MYSQL* mysql = static_cast<MYSQL*>(connection_);

		// Execute query
		if (mysql_query(mysql, query_string.c_str()) != 0) {
			last_error_ = std::string("Query failed: ") + mysql_error(mysql);
			logger_.error("select_query", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"mysql_backend"
			};
		}

		// Get result set
		MYSQL_RES* res = mysql_store_result(mysql);
		if (!res) {
			if (mysql_field_count(mysql) == 0) {
				// Query was not a SELECT
				last_error_.clear();
				return result;
			} else {
				last_error_ = std::string("Result retrieval failed: ") + mysql_error(mysql);
				logger_.error("select_query", last_error_);
				return kcenon::common::error_info{
					static_cast<int>(database::error_code::query_failed),
					last_error_,
					"mysql_backend"
				};
			}
		}

		// Get field information
		MYSQL_FIELD* fields = mysql_fetch_fields(res);
		unsigned int num_fields = mysql_num_fields(res);

		// Process rows
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(res))) {
			core::database_row db_row;
			unsigned long* lengths = mysql_fetch_lengths(res);

			for (unsigned int i = 0; i < num_fields; i++) {
				std::string field_name = fields[i].name;

				if (row[i] == nullptr) {
					db_row[field_name] = nullptr;
				} else {
					// Convert MySQL types to database_value
					switch (fields[i].type) {
					case MYSQL_TYPE_TINY:
					case MYSQL_TYPE_SHORT:
					case MYSQL_TYPE_LONG:
					case MYSQL_TYPE_LONGLONG:
					case MYSQL_TYPE_INT24:
						db_row[field_name] = static_cast<int64_t>(std::stoll(row[i]));
						break;
					case MYSQL_TYPE_DECIMAL:
					case MYSQL_TYPE_NEWDECIMAL:
					case MYSQL_TYPE_FLOAT:
					case MYSQL_TYPE_DOUBLE:
						db_row[field_name] = std::stod(row[i]);
						break;
					case MYSQL_TYPE_BIT:
						db_row[field_name] = (row[i][0] != '0');
						break;
					default:
						db_row[field_name] = std::string(row[i], lengths[i]);
						break;
					}
				}
			}
			result.push_back(std::move(db_row));
		}

		mysql_free_result(res);
	} catch (const std::exception& e) {
		last_error_ = std::string("Query error: ") + e.what();
		logger_.error("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}
#else
	logger_.warning("MySQL support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	if (query_string.find("SELECT") != std::string::npos) {
		core::database_row mock_row;
		mock_row["id"] = int64_t(1);
		mock_row["name"] = std::string("mysql_mock_data");
		mock_row["active"] = true;
		result.push_back(mock_row);
	}
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult mysql_backend::execute_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

#ifdef USE_MYSQL
	if (!connection_) {
		last_error_ = "No active MySQL connection";
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mysql_backend"
		};
	}

	if (mysql_query(static_cast<MYSQL*>(connection_), query_string.c_str()) != 0) {
		last_error_ = std::string("Execute error: ") + mysql_error(static_cast<MYSQL*>(connection_));
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}

	last_error_.clear();
	return kcenon::common::ok();
#else
	// Mock execution
	logger_.info("MySQL support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::VoidResult mysql_backend::begin_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	auto result = execute_query("START TRANSACTION");
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult mysql_backend::commit_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	auto result = execute_query("COMMIT");
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult mysql_backend::rollback_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	if (!in_transaction_) {
		// Not an error - already rolled back or never started
		return kcenon::common::ok();
	}

	auto result = execute_query("ROLLBACK");
	in_transaction_ = false; // Force state reset even on error

	if (result.is_err()) {
		return result;
	}

	last_error_.clear();
	return kcenon::common::ok();
}

bool mysql_backend::in_transaction() const
{
	return in_transaction_;
}

std::string mysql_backend::last_error() const
{
	return last_error_;
}

std::map<std::string, std::string> mysql_backend::connection_info() const
{
	std::map<std::string, std::string> info;
	info["backend"] = "mysql";
	info["host"] = connection_config_.host;
	info["port"] = std::to_string(connection_config_.port);
	info["database"] = connection_config_.database;
	info["username"] = connection_config_.username;
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when MySQL support is compiled in
#ifdef USE_MYSQL
namespace {
	database::core::backend_registrar<database::backends::mysql_backend> mysql_registrar("mysql");
}
#endif
