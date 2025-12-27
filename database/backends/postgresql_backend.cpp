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

#include "postgresql_backend.h"
#include "../core/result.h"

#ifdef USE_POSTGRESQL
#include <pqxx/pqxx>
#elif defined(HAVE_LIBPQ)
#include "libpq-fe.h"
#endif

#include <sstream>
#include <iomanip>
#include <variant>
#include <iostream>

// Logging helper macros
#define POSTGRES_LOG_ERROR(context, message) \
	std::cerr << "[PostgreSQL:" << context << "] Error: " << message << std::endl
#define POSTGRES_LOG_WARNING(message) \
	std::cerr << "[PostgreSQL] Warning: " << message << std::endl
#define POSTGRES_LOG_INFO(message) \
	std::cout << "[PostgreSQL] Info: " << message << std::endl

namespace database
{
namespace backends
{

postgresql_backend::postgresql_backend()
	: connection_(nullptr)
{
}

postgresql_backend::~postgresql_backend()
{
	shutdown();
}

std::unique_ptr<core::database_backend> postgresql_backend::create()
{
	return std::make_unique<postgresql_backend>();
}

database_types postgresql_backend::type() const
{
	return database_types::postgres;
}

kcenon::common::VoidResult postgresql_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			"Backend already initialized",
			"postgresql_backend"
		};
	}

	connection_config_ = config;
	std::string conn_str = build_connection_string(config);

#ifdef USE_POSTGRESQL
	try {
		auto conn = std::make_unique<pqxx::connection>(conn_str);
		if (conn->is_open()) {
			connection_ = conn.release();
			initialized_ = true;
			last_error_.clear();
			return kcenon::common::ok();
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		POSTGRES_LOG_ERROR("initialize", last_error_);
	}
#elif defined(HAVE_LIBPQ)
	try {
		connection_ = PQconnectdb(conn_str.c_str());
		if (PQstatus(static_cast<PGconn*>(connection_)) == CONNECTION_OK) {
			initialized_ = true;
			last_error_.clear();
			return kcenon::common::ok();
		}
		last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
		PQfinish(static_cast<PGconn*>(connection_));
		connection_ = nullptr;
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		POSTGRES_LOG_ERROR("initialize", last_error_);
	}
#else
	POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Connection: " + conn_str.substr(0, 20) + "...");
	// Mock mode for testing without PostgreSQL
	initialized_ = true;
	last_error_.clear();
	return kcenon::common::ok();
#endif

	if (last_error_.empty()) {
		last_error_ = "Failed to connect to PostgreSQL server";
	}
	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"postgresql_backend"
	};
}

kcenon::common::VoidResult postgresql_backend::shutdown()
{
	if (!initialized_) {
		return kcenon::common::ok(); // Already shutdown
	}

	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_POSTGRESQL
	try {
		delete static_cast<pqxx::connection*>(connection_);
		connection_ = nullptr;
		initialized_ = false;
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Disconnect error: ") + e.what();
		POSTGRES_LOG_ERROR("shutdown", last_error_);
	}
#elif defined(HAVE_LIBPQ)
	try {
		PQfinish(static_cast<PGconn*>(connection_));
		connection_ = nullptr;
		initialized_ = false;
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Disconnect error: ") + e.what();
		POSTGRES_LOG_ERROR("shutdown", last_error_);
	}
#else
	connection_ = nullptr;
	initialized_ = false;
	last_error_.clear();
	return kcenon::common::ok();
#endif

	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"postgresql_backend"
	};
}

bool postgresql_backend::is_initialized() const
{
	return initialized_;
}

unsigned int postgresql_backend::execute_modification_query(const std::string& query_string)
{
#ifdef USE_POSTGRESQL
	if (!connection_) return 0;
	try {
		pqxx::connection* conn = static_cast<pqxx::connection*>(connection_);
		pqxx::work txn(*conn);
		pqxx::result result = txn.exec(query_string);
		txn.commit();
		return static_cast<unsigned int>(result.affected_rows());
	} catch (const std::exception& e) {
		last_error_ = std::string("Modification query error: ") + e.what();
		POSTGRES_LOG_ERROR("execute_modification_query", last_error_);
	}
#elif defined(HAVE_LIBPQ)
	if (!connection_) return 0;
	try {
		PGresult* result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
			PQclear(result);
			return 0;
		}
		const char* affected_rows = PQcmdTuples(result);
		unsigned int count = 0;
		if (affected_rows && *affected_rows) {
			count = static_cast<unsigned int>(std::stoul(affected_rows));
		}
		PQclear(result);
		return count;
	} catch (const std::exception& e) {
		last_error_ = std::string("Modification query error: ") + e.what();
		POSTGRES_LOG_ERROR("execute_modification_query", last_error_);
	}
#else
	POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Modification query: " + query_string.substr(0, 20) + "...");
	return 1; // Mock: return 1 affected row
#endif
	return 0;
}

kcenon::common::Result<uint64_t> postgresql_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> postgresql_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> postgresql_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	unsigned int affected = execute_modification_query(query_string);
	if (!last_error_.empty()) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<database_result> postgresql_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	database_result result;

#ifdef USE_POSTGRESQL
	if (!connection_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"postgresql_backend"
		};
	}
	try {
		pqxx::connection* conn = static_cast<pqxx::connection*>(connection_);
		pqxx::work txn(*conn);
		pqxx::result pqxx_result = txn.exec(query_string);
		txn.commit();

		for (const auto& row : pqxx_result) {
			database_row db_row;
			for (size_t i = 0; i < row.size(); ++i) {
				std::string column_name = pqxx_result.column_name(i);
				if (row[i].is_null()) {
					db_row[column_name] = nullptr;
				} else {
					// Try to convert to appropriate type
					if (row[i].type() == pqxx::oid::int8_oid ||
						row[i].type() == pqxx::oid::int4_oid) {
						db_row[column_name] = row[i].as<int64_t>();
					} else if (row[i].type() == pqxx::oid::float8_oid ||
							   row[i].type() == pqxx::oid::float4_oid) {
						db_row[column_name] = row[i].as<double>();
					} else if (row[i].type() == pqxx::oid::bool_oid) {
						db_row[column_name] = row[i].as<bool>();
					} else {
						db_row[column_name] = row[i].as<std::string>();
					}
				}
			}
			result.push_back(std::move(db_row));
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Select query error: ") + e.what();
		POSTGRES_LOG_ERROR("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#elif defined(HAVE_LIBPQ)
	if (!connection_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"postgresql_backend"
		};
	}
	try {
		PGresult* pg_result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
		if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
			last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
			PQclear(pg_result);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"postgresql_backend"
			};
		}

		int rows = PQntuples(pg_result);
		int cols = PQnfields(pg_result);

		for (int row = 0; row < rows; ++row) {
			database_row db_row;
			for (int col = 0; col < cols; ++col) {
				std::string column_name = PQfname(pg_result, col);
				if (PQgetisnull(pg_result, row, col)) {
					db_row[column_name] = nullptr;
				} else {
					const char* value = PQgetvalue(pg_result, row, col);
					Oid type = PQftype(pg_result, col);

					// Convert based on PostgreSQL type
					if (type == 20 || type == 21 || type == 23) { // int8, int2, int4
						db_row[column_name] = static_cast<int64_t>(std::stoll(value));
					} else if (type == 700 || type == 701) { // float4, float8
						db_row[column_name] = std::stod(value);
					} else if (type == 16) { // bool
						db_row[column_name] = (*value == 't' || *value == '1');
					} else {
						db_row[column_name] = std::string(value);
					}
				}
			}
			result.push_back(std::move(db_row));
		}
		PQclear(pg_result);
	} catch (const std::exception& e) {
		last_error_ = std::string("Select query error: ") + e.what();
		POSTGRES_LOG_ERROR("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#else
	POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	if (query_string.find("SELECT") != std::string::npos) {
		database_row mock_row;
		mock_row["id"] = int64_t(1);
		mock_row["name"] = std::string("mock_data");
		mock_row["active"] = true;
		result.push_back(mock_row);
	}
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult postgresql_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

#ifdef USE_POSTGRESQL
	try {
		if (!connection_) {
			last_error_ = "No active PostgreSQL connection";
			POSTGRES_LOG_ERROR("execute_query", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"postgresql_backend"
			};
		}

		pqxx::work txn{*static_cast<pqxx::connection*>(connection_)};
		txn.exec(query_string);
		txn.commit();
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Execute error: ") + e.what();
		POSTGRES_LOG_ERROR("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#elif defined(HAVE_LIBPQ)
	if (!connection_) {
		last_error_ = "No active PostgreSQL connection";
		POSTGRES_LOG_ERROR("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	PGresult* result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
	if (result == nullptr) {
		last_error_ = "PostgreSQL execute failed";
		POSTGRES_LOG_ERROR("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	ExecStatusType status = PQresultStatus(result);
	bool success = (status == PGRES_COMMAND_OK) || (status == PGRES_TUPLES_OK);

	if (!success) {
		last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
		POSTGRES_LOG_ERROR("execute_query", last_error_);
		PQclear(result);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	PQclear(result);
	last_error_.clear();
	return kcenon::common::ok();
#else
	// Mock execution
	POSTGRES_LOG_INFO("PostgreSQL support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::VoidResult postgresql_backend::begin_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	auto result = execute_query("BEGIN");
	if (!result) {
		return result;
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult postgresql_backend::commit_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	auto result = execute_query("COMMIT");
	if (!result) {
		return result;
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult postgresql_backend::rollback_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	if (!in_transaction_) {
		// Not an error - already rolled back or never started
		return kcenon::common::ok();
	}

	auto result = execute_query("ROLLBACK");
	in_transaction_ = false; // Force state reset even on error

	if (!result) {
		return result;
	}

	last_error_.clear();
	return kcenon::common::ok();
}

bool postgresql_backend::in_transaction() const
{
	return in_transaction_;
}

std::string postgresql_backend::last_error() const
{
	return last_error_;
}

std::map<std::string, std::string> postgresql_backend::connection_info() const
{
	std::map<std::string, std::string> info;
	info["backend"] = "postgresql";
	info["host"] = connection_config_.host;
	info["port"] = std::to_string(connection_config_.port);
	info["database"] = connection_config_.database;
	info["username"] = connection_config_.username;
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

std::string postgresql_backend::build_connection_string(const core::connection_config& config) const
{
	std::ostringstream oss;

	if (!config.host.empty()) {
		oss << "host=" << config.host << " ";
	}

	if (config.port > 0) {
		oss << "port=" << config.port << " ";
	}

	if (!config.database.empty()) {
		oss << "dbname=" << config.database << " ";
	}

	if (!config.username.empty()) {
		oss << "user=" << config.username << " ";
	}

	if (!config.password.empty()) {
		oss << "password=" << config.password << " ";
	}

	// Append any additional options
	for (const auto& [key, value] : config.options) {
		oss << key << "=" << value << " ";
	}

	return oss.str();
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when PostgreSQL support is compiled in
#ifdef USE_POSTGRESQL
namespace {
	database::core::backend_registrar<database::backends::postgresql_backend> postgresql_registrar("postgresql");
}
#endif
