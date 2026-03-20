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

#include "postgres_manager.h"

#ifdef USE_POSTGRESQL
#include <pqxx/pqxx>
#elif defined(HAVE_LIBPQ)
#include "libpq-fe.h"
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>

// Logging helper macros - using std::cerr/cout for consistent behavior
#define POSTGRES_LOG_ERROR(context, message) \
	std::cerr << "[PostgreSQL:" << context << "] Error: " << message << std::endl
#define POSTGRES_LOG_WARNING(message) \
	std::cerr << "[PostgreSQL] Warning: " << message << std::endl
#define POSTGRES_LOG_INFO(message) \
	std::cout << "[PostgreSQL] Info: " << message << std::endl

namespace database
{
	postgres_manager::postgres_manager(void)
		: connection_(nullptr)
		, initialized_(false)
		, in_transaction_(false)
	{
	}

	postgres_manager::~postgres_manager(void)
	{
		shutdown();
	}

	database_types postgres_manager::type() const
	{
		return database_types::postgres;
	}

	kcenon::common::VoidResult postgres_manager::initialize(const core::connection_config& config)
	{
		if (initialized_) {
			return kcenon::common::error_info{-1, "Already initialized", "postgres_manager"};
		}

		// Build connection string from config
		std::ostringstream conn_str;
		if (!config.host.empty()) {
			conn_str << "host=" << config.host << " ";
		}
		if (config.port > 0) {
			conn_str << "port=" << config.port << " ";
		}
		if (!config.database.empty()) {
			conn_str << "dbname=" << config.database << " ";
		}
		if (!config.username.empty()) {
			conn_str << "user=" << config.username << " ";
		}
		if (!config.password.empty()) {
			conn_str << "password=" << config.password << " ";
		}
		for (const auto& [key, value] : config.options) {
			conn_str << key << "=" << value << " ";
		}

		connection_string_ = conn_str.str();

#ifdef USE_POSTGRESQL
		try {
			auto conn = std::make_unique<pqxx::connection>(connection_string_);
			if (conn->is_open()) {
				connection_ = conn.release();
				initialized_ = true;
				return kcenon::common::ok();
			}
			last_error_ = "Connection failed to open";
		} catch (const std::exception& e) {
			last_error_ = std::string("Connection error: ") + e.what();
			POSTGRES_LOG_ERROR("initialize", last_error_);
		}
#elif defined(HAVE_LIBPQ)
		try {
			connection_ = PQconnectdb(connection_string_.c_str());
			if (PQstatus(static_cast<PGconn*>(connection_)) == CONNECTION_OK) {
				initialized_ = true;
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
		POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Mock connection established.");
		initialized_ = true;
		return kcenon::common::ok();
#endif
		return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
	}

	kcenon::common::VoidResult postgres_manager::shutdown()
	{
		if (!initialized_) {
			return kcenon::common::ok();
		}

		if (in_transaction_) {
			rollback_transaction();
		}

#ifdef USE_POSTGRESQL
		try {
			delete static_cast<pqxx::connection*>(connection_);
			connection_ = nullptr;
			initialized_ = false;
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
			return kcenon::common::ok();
		} catch (const std::exception& e) {
			last_error_ = std::string("Disconnect error: ") + e.what();
			POSTGRES_LOG_ERROR("shutdown", last_error_);
		}
#else
		connection_ = nullptr;
		initialized_ = false;
		POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Mock disconnect.");
		return kcenon::common::ok();
#endif
		return kcenon::common::error_info{-3, last_error_, "postgres_manager"};
	}

	bool postgres_manager::is_initialized() const
	{
		return initialized_;
	}

	kcenon::common::Result<uint64_t> postgres_manager::execute_modification_query(const std::string& query_string)
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

#ifdef USE_POSTGRESQL
		try {
			pqxx::connection* conn = static_cast<pqxx::connection*>(connection_);
			pqxx::work txn(*conn);
			pqxx::result result = txn.exec(query_string);
			txn.commit();
			return static_cast<uint64_t>(result.affected_rows());
		} catch (const std::exception& e) {
			last_error_ = std::string("Modification query error: ") + e.what();
			POSTGRES_LOG_ERROR("execute_modification_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}
#elif defined(HAVE_LIBPQ)
		try {
			PGresult* result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
			if (PQresultStatus(result) != PGRES_COMMAND_OK) {
				last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
				PQclear(result);
				return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
			}
			const char* affected_rows = PQcmdTuples(result);
			uint64_t count = 0;
			if (affected_rows && *affected_rows) {
				count = static_cast<uint64_t>(std::stoull(affected_rows));
			}
			PQclear(result);
			return count;
		} catch (const std::exception& e) {
			last_error_ = std::string("Modification query error: ") + e.what();
			POSTGRES_LOG_ERROR("execute_modification_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}
#else
		POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Mock modification: " + query_string.substr(0, 20) + "...");
		return uint64_t{1};
#endif
	}

	kcenon::common::Result<core::database_result> postgres_manager::select_query(const std::string& query_string)
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

		core::database_result result;

#ifdef USE_POSTGRESQL
		try {
			pqxx::connection* conn = static_cast<pqxx::connection*>(connection_);
			pqxx::work txn(*conn);
			pqxx::result pqxx_result = txn.exec(query_string);
			txn.commit();

			for (const auto& row : pqxx_result) {
				core::database_row db_row;
				for (size_t i = 0; i < row.size(); ++i) {
					std::string column_name = pqxx_result.column_name(i);
					if (row[i].is_null()) {
						db_row[column_name] = nullptr;
					} else {
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
			return result;
		} catch (const std::exception& e) {
			last_error_ = std::string("Select query error: ") + e.what();
			POSTGRES_LOG_ERROR("select_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}
#elif defined(HAVE_LIBPQ)
		try {
			PGresult* pg_result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
			if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
				last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
				PQclear(pg_result);
				return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
			}

			int rows = PQntuples(pg_result);
			int cols = PQnfields(pg_result);

			for (int row = 0; row < rows; ++row) {
				core::database_row db_row;
				for (int col = 0; col < cols; ++col) {
					std::string column_name = PQfname(pg_result, col);
					if (PQgetisnull(pg_result, row, col)) {
						db_row[column_name] = nullptr;
					} else {
						const char* value = PQgetvalue(pg_result, row, col);
						Oid type_oid = PQftype(pg_result, col);

						if (type_oid == 20 || type_oid == 21 || type_oid == 23) { // int8, int2, int4
							db_row[column_name] = static_cast<int64_t>(std::stoll(value));
						} else if (type_oid == 700 || type_oid == 701) { // float4, float8
							db_row[column_name] = std::stod(value);
						} else if (type_oid == 16) { // bool
							db_row[column_name] = (*value == 't' || *value == '1');
						} else {
							db_row[column_name] = std::string(value);
						}
					}
				}
				result.push_back(std::move(db_row));
			}
			PQclear(pg_result);
			return result;
		} catch (const std::exception& e) {
			last_error_ = std::string("Select query error: ") + e.what();
			POSTGRES_LOG_ERROR("select_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}
#else
		POSTGRES_LOG_WARNING("PostgreSQL support not compiled. Mock select: " + query_string.substr(0, 20) + "...");
		if (query_string.find("SELECT") != std::string::npos) {
			core::database_row mock_row;
			mock_row["id"] = int64_t(1);
			mock_row["name"] = std::string("mock_data");
			mock_row["active"] = true;
			result.push_back(mock_row);
		}
		return result;
#endif
	}

	kcenon::common::VoidResult postgres_manager::execute_query(const std::string& query_string)
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

#ifdef USE_POSTGRESQL
		try {
			pqxx::work txn{*static_cast<pqxx::connection*>(connection_)};
			txn.exec(query_string);
			txn.commit();
			return kcenon::common::ok();
		} catch (const std::exception& e) {
			last_error_ = std::string("Execute error: ") + e.what();
			POSTGRES_LOG_ERROR("execute_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}
#elif defined(HAVE_LIBPQ)
		PGresult* result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
		if (result == nullptr) {
			last_error_ = "PostgreSQL execute failed";
			POSTGRES_LOG_ERROR("execute_query", last_error_);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}

		ExecStatusType status = PQresultStatus(result);
		bool success = (status == PGRES_COMMAND_OK) || (status == PGRES_TUPLES_OK);

		if (!success) {
			last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
			POSTGRES_LOG_ERROR("execute_query", last_error_);
			PQclear(result);
			return kcenon::common::error_info{-2, last_error_, "postgres_manager"};
		}

		PQclear(result);
		return kcenon::common::ok();
#else
		POSTGRES_LOG_INFO("PostgreSQL support not compiled. Mock execute: " + query_string);
		return kcenon::common::ok();
#endif
	}

	kcenon::common::VoidResult postgres_manager::begin_transaction()
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

		if (in_transaction_) {
			return kcenon::common::error_info{-2, "Transaction already active", "postgres_manager"};
		}

		auto result = execute_query("BEGIN");
		if (result.is_ok()) {
			in_transaction_ = true;
		}
		return result;
	}

	kcenon::common::VoidResult postgres_manager::commit_transaction()
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

		if (!in_transaction_) {
			return kcenon::common::error_info{-2, "No active transaction", "postgres_manager"};
		}

		auto result = execute_query("COMMIT");
		if (result.is_ok()) {
			in_transaction_ = false;
		}
		return result;
	}

	kcenon::common::VoidResult postgres_manager::rollback_transaction()
	{
		if (!initialized_) {
			return kcenon::common::error_info{-1, "Not initialized", "postgres_manager"};
		}

		if (!in_transaction_) {
			return kcenon::common::error_info{-2, "No active transaction", "postgres_manager"};
		}

		auto result = execute_query("ROLLBACK");
		if (result.is_ok()) {
			in_transaction_ = false;
		}
		return result;
	}

	bool postgres_manager::in_transaction() const
	{
		return in_transaction_;
	}

	std::string postgres_manager::last_error() const
	{
		return last_error_;
	}

	std::map<std::string, std::string> postgres_manager::connection_info() const
	{
		std::map<std::string, std::string> info;
		info["backend"] = "postgresql";
		info["initialized"] = initialized_ ? "true" : "false";
		info["in_transaction"] = in_transaction_ ? "true" : "false";

#ifdef USE_POSTGRESQL
		info["driver"] = "libpqxx";
		if (initialized_ && connection_) {
			auto* conn = static_cast<pqxx::connection*>(connection_);
			info["server_version"] = std::to_string(conn->server_version());
			info["protocol_version"] = std::to_string(conn->protocol_version());
		}
#elif defined(HAVE_LIBPQ)
		info["driver"] = "libpq";
		if (initialized_ && connection_) {
			auto* conn = static_cast<PGconn*>(connection_);
			info["server_version"] = std::to_string(PQserverVersion(conn));
			info["protocol_version"] = std::to_string(PQprotocolVersion(conn));
		}
#else
		info["driver"] = "mock";
#endif

		return info;
	}
} // namespace database
