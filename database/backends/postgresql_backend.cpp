// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
#include <cctype>
#include <cstring>

#include "../utils/backend_logger.h"

namespace
{
const database::utils::backend_logger logger_("PostgreSQL");

// PostgreSQL type OIDs (from pg_type.h)
constexpr unsigned int PG_INT4OID = 23;
constexpr unsigned int PG_INT8OID = 20;
constexpr unsigned int PG_FLOAT4OID = 700;
constexpr unsigned int PG_FLOAT8OID = 701;
constexpr unsigned int PG_BOOLOID = 16;

// Case-insensitive check: does `query` begin with `keyword` (surrounded by
// whitespace/EOS/semicolon)? Used to detect BEGIN / COMMIT / ROLLBACK /
// ROLLBACK TO ... transaction-control statements in execute_query so the
// persistent pqxx::work can be opened, committed, or aborted across calls.
bool query_starts_with(const std::string& query, const char* keyword) {
	const std::size_t n = std::strlen(keyword);
	std::size_t i = 0;
	while (i < query.size() &&
	       std::isspace(static_cast<unsigned char>(query[i]))) {
		++i;
	}
	if (query.size() - i < n) {
		return false;
	}
	for (std::size_t k = 0; k < n; ++k) {
		if (std::tolower(static_cast<unsigned char>(query[i + k])) !=
		    std::tolower(static_cast<unsigned char>(keyword[k]))) {
			return false;
		}
	}
	if (i + n == query.size()) {
		return true;
	}
	const char c = query[i + n];
	return std::isspace(static_cast<unsigned char>(c)) || c == ';';
}
}

namespace database
{
namespace backends
{

postgresql_backend::postgresql_backend()
	: connection_(nullptr)
{
}

kcenon::common::VoidResult postgresql_backend::do_initialize(const core::connection_config& config)
{
	connection_config_ = config;
	std::string conn_str = build_connection_string(config);

#ifdef USE_POSTGRESQL
	try {
		auto conn = std::make_unique<pqxx::connection>(conn_str);
		if (conn->is_open()) {
			connection_ = conn.release();
			last_error_.clear();
			return kcenon::common::ok();
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + sanitize_error(e.what());
		logger_.error("do_initialize", last_error_);
	}
#elif defined(HAVE_LIBPQ)
	try {
		connection_ = PQconnectdb(conn_str.c_str());
		if (PQstatus(static_cast<PGconn*>(connection_)) == CONNECTION_OK) {
			last_error_.clear();
			return kcenon::common::ok();
		}
		last_error_ = sanitize_error(PQerrorMessage(static_cast<PGconn*>(connection_)));
		PQfinish(static_cast<PGconn*>(connection_));
		connection_ = nullptr;
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + sanitize_error(e.what());
		logger_.error("do_initialize", last_error_);
	}
#else
	logger_.warning("PostgreSQL support not compiled. Connection: " + build_safe_connection_string(config));
	// Mock mode for testing without PostgreSQL
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

kcenon::common::VoidResult postgresql_backend::do_shutdown()
{
	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_POSTGRESQL
	// Safety: if rollback_transaction() couldn't reach the connection for
	// any reason, drop the active pqxx::work* here before the connection
	// underneath it is deleted.
	if (active_txn_ != nullptr) {
		try {
			auto* txn = static_cast<pqxx::work*>(active_txn_);
			txn->abort();
			delete txn;
		} catch (...) {
			// Swallow — the connection teardown below is the recovery path.
		}
		active_txn_ = nullptr;
		in_transaction_ = false;
	}

	try {
		delete static_cast<pqxx::connection*>(connection_);
		connection_ = nullptr;
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Disconnect error: ") + e.what();
		logger_.error("do_shutdown", last_error_);
	}
#elif defined(HAVE_LIBPQ)
	try {
		PQfinish(static_cast<PGconn*>(connection_));
		connection_ = nullptr;
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Disconnect error: ") + e.what();
		logger_.error("do_shutdown", last_error_);
	}
#else
	connection_ = nullptr;
	last_error_.clear();
	return kcenon::common::ok();
#endif

	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"postgresql_backend"
	};
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
		logger_.error("execute_modification_query", last_error_);
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
		logger_.error("execute_modification_query", last_error_);
	}
#else
	logger_.warning("PostgreSQL support not compiled. Modification query: " + query_string.substr(0, 20) + "...");
	return 1; // Mock: return 1 affected row
#endif
	return 0;
}



kcenon::common::Result<core::database_result> postgresql_backend::select_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	core::database_result result;

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
			core::database_row db_row;
			for (size_t i = 0; i < row.size(); ++i) {
				std::string column_name = pqxx_result.column_name(i);
				if (row[i].is_null()) {
					db_row[column_name] = nullptr;
				} else {
					// Try to convert to appropriate type
					if (row[i].type() == PG_INT8OID ||
						row[i].type() == PG_INT4OID) {
						db_row[column_name] = row[i].as<int64_t>();
					} else if (row[i].type() == PG_FLOAT8OID ||
							   row[i].type() == PG_FLOAT4OID) {
						db_row[column_name] = row[i].as<double>();
					} else if (row[i].type() == PG_BOOLOID) {
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
		logger_.error("select_query", last_error_);
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
			core::database_row db_row;
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
		logger_.error("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#else
	logger_.warning("PostgreSQL support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	if (query_string.find("SELECT") != std::string::npos) {
		core::database_row mock_row;
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
	if (!is_initialized()) {
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
			logger_.error("execute_query", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"postgresql_backend"
			};
		}

		auto* conn = static_cast<pqxx::connection*>(connection_);

		// Route through a persistent pqxx::work when a multi-statement txn
		// is open, so BEGIN / INSERT / COMMIT issued as separate
		// execute_query calls compose into a single transaction. Without
		// this, each call would open-and-commit its own pqxx::work,
		// silently auto-committing every statement and breaking
		// ROLLBACK / SAVEPOINT / isolation semantics (#572).
		if (active_txn_ != nullptr) {
			auto* txn = static_cast<pqxx::work*>(active_txn_);
			if (query_starts_with(query_string, "COMMIT") ||
			    query_starts_with(query_string, "END")) {
				txn->commit();
				delete txn;
				active_txn_ = nullptr;
				in_transaction_ = false;
			} else if (query_starts_with(query_string, "ROLLBACK") &&
			           !query_starts_with(query_string, "ROLLBACK TO")) {
				txn->abort();
				delete txn;
				active_txn_ = nullptr;
				in_transaction_ = false;
			} else {
				// SAVEPOINT, RELEASE SAVEPOINT, ROLLBACK TO SAVEPOINT,
				// and all DML execute within the active txn.
				txn->exec(query_string);
			}
		} else {
			if (query_starts_with(query_string, "BEGIN") ||
			    query_starts_with(query_string, "START")) {
				// Open a persistent txn. pqxx::work's ctor issues BEGIN
				// internally, so we do not forward the BEGIN query text.
				active_txn_ = new pqxx::work{*conn};
				in_transaction_ = true;
			} else {
				// Transient auto-transaction (previous behavior).
				pqxx::work txn{*conn};
				txn.exec(query_string);
				txn.commit();
			}
		}

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		// If the active txn is now in a poisoned state (pqxx throws on
		// committed-or-aborted work, or on a failed exec), drop it so the
		// next BEGIN can open a fresh one.
		if (active_txn_ != nullptr) {
			try {
				delete static_cast<pqxx::work*>(active_txn_);
			} catch (...) {
				// Swallow secondary teardown error; we report the primary.
			}
			active_txn_ = nullptr;
			in_transaction_ = false;
		}
		last_error_ = std::string("Execute error: ") + e.what();
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#elif defined(HAVE_LIBPQ)
	if (!connection_) {
		last_error_ = "No active PostgreSQL connection";
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	PGresult* result = PQexec(static_cast<PGconn*>(connection_), query_string.c_str());
	if (result == nullptr) {
		last_error_ = "PostgreSQL execute failed";
		logger_.error("execute_query", last_error_);
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
		logger_.error("execute_query", last_error_);
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
	logger_.info("PostgreSQL support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::Result<core::database_result> postgresql_backend::select_prepared(
	const std::string& query,
	const std::vector<core::database_value>& params)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	core::database_result result;

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

		// Build pqxx::params from database_value vector
		pqxx::params pq_params;
		for (const auto& val : params) {
			std::visit([&pq_params](const auto& v) {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, std::nullptr_t>) {
					pq_params.append();
				} else if constexpr (std::is_same_v<T, bool>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, int64_t>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, double>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, std::string>) {
					pq_params.append(v);
				}
			}, val);
		}

		pqxx::result pqxx_result = txn.exec_params(query, pq_params);
		txn.commit();

		for (const auto& row : pqxx_result) {
			core::database_row db_row;
			for (size_t i = 0; i < row.size(); ++i) {
				std::string column_name = pqxx_result.column_name(i);
				if (row[i].is_null()) {
					db_row[column_name] = nullptr;
				} else {
					if (row[i].type() == PG_INT8OID ||
						row[i].type() == PG_INT4OID) {
						db_row[column_name] = row[i].as<int64_t>();
					} else if (row[i].type() == PG_FLOAT8OID ||
							   row[i].type() == PG_FLOAT4OID) {
						db_row[column_name] = row[i].as<double>();
					} else if (row[i].type() == PG_BOOLOID) {
						db_row[column_name] = row[i].as<bool>();
					} else {
						db_row[column_name] = row[i].as<std::string>();
					}
				}
			}
			result.push_back(std::move(db_row));
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Select prepared error: ") + e.what();
		logger_.error("select_prepared", last_error_);
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
		// Convert params to C string array for PQexecParams
		std::vector<std::string> param_strings;
		std::vector<const char*> param_values;
		param_strings.reserve(params.size());
		param_values.reserve(params.size());

		for (const auto& val : params) {
			std::visit([&param_strings, &param_values](const auto& v) {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, std::nullptr_t>) {
					param_strings.emplace_back();
					param_values.push_back(nullptr);
				} else if constexpr (std::is_same_v<T, bool>) {
					param_strings.push_back(v ? "t" : "f");
					param_values.push_back(param_strings.back().c_str());
				} else if constexpr (std::is_same_v<T, std::string>) {
					param_strings.push_back(v);
					param_values.push_back(param_strings.back().c_str());
				} else {
					param_strings.push_back(std::to_string(v));
					param_values.push_back(param_strings.back().c_str());
				}
			}, val);
		}

		PGresult* pg_result = PQexecParams(
			static_cast<PGconn*>(connection_),
			query.c_str(),
			static_cast<int>(params.size()),
			nullptr,  // let server infer types
			param_values.data(),
			nullptr,  // text format lengths
			nullptr,  // text format
			0         // text result format
		);

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
			core::database_row db_row;
			for (int col = 0; col < cols; ++col) {
				std::string column_name = PQfname(pg_result, col);
				if (PQgetisnull(pg_result, row, col)) {
					db_row[column_name] = nullptr;
				} else {
					const char* value = PQgetvalue(pg_result, row, col);
					Oid type = PQftype(pg_result, col);

					if (type == 20 || type == 21 || type == 23) {
						db_row[column_name] = static_cast<int64_t>(std::stoll(value));
					} else if (type == 700 || type == 701) {
						db_row[column_name] = std::stod(value);
					} else if (type == 16) {
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
		last_error_ = std::string("Select prepared error: ") + e.what();
		logger_.error("select_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#else
	// Fallback to string interpolation for mock mode
	return database_backend::select_prepared(query, params);
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult postgresql_backend::execute_prepared(
	const std::string& query,
	const std::vector<core::database_value>& params)
{
	if (!is_initialized()) {
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
			logger_.error("execute_prepared", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"postgresql_backend"
			};
		}

		pqxx::connection* conn = static_cast<pqxx::connection*>(connection_);
		pqxx::work txn(*conn);

		pqxx::params pq_params;
		for (const auto& val : params) {
			std::visit([&pq_params](const auto& v) {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, std::nullptr_t>) {
					pq_params.append();
				} else if constexpr (std::is_same_v<T, bool>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, int64_t>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, double>) {
					pq_params.append(v);
				} else if constexpr (std::is_same_v<T, std::string>) {
					pq_params.append(v);
				}
			}, val);
		}

		txn.exec_params(query, pq_params);
		txn.commit();
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Execute prepared error: ") + e.what();
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}
#elif defined(HAVE_LIBPQ)
	if (!connection_) {
		last_error_ = "No active PostgreSQL connection";
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	std::vector<std::string> param_strings;
	std::vector<const char*> param_values;
	param_strings.reserve(params.size());
	param_values.reserve(params.size());

	for (const auto& val : params) {
		std::visit([&param_strings, &param_values](const auto& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::nullptr_t>) {
				param_strings.emplace_back();
				param_values.push_back(nullptr);
			} else if constexpr (std::is_same_v<T, bool>) {
				param_strings.push_back(v ? "t" : "f");
				param_values.push_back(param_strings.back().c_str());
			} else if constexpr (std::is_same_v<T, std::string>) {
				param_strings.push_back(v);
				param_values.push_back(param_strings.back().c_str());
			} else {
				param_strings.push_back(std::to_string(v));
				param_values.push_back(param_strings.back().c_str());
			}
		}, val);
	}

	PGresult* pg_result = PQexecParams(
		static_cast<PGconn*>(connection_),
		query.c_str(),
		static_cast<int>(params.size()),
		nullptr,
		param_values.data(),
		nullptr,
		nullptr,
		0
	);

	if (pg_result == nullptr) {
		last_error_ = "PostgreSQL execute prepared failed";
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	ExecStatusType status = PQresultStatus(pg_result);
	bool success = (status == PGRES_COMMAND_OK) || (status == PGRES_TUPLES_OK);

	if (!success) {
		last_error_ = PQerrorMessage(static_cast<PGconn*>(connection_));
		logger_.error("execute_prepared", last_error_);
		PQclear(pg_result);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"postgresql_backend"
		};
	}

	PQclear(pg_result);
	last_error_.clear();
	return kcenon::common::ok();
#else
	return database_backend::execute_prepared(query, params);
#endif
}

kcenon::common::VoidResult postgresql_backend::begin_transaction()
{
	if (!is_initialized()) {
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
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult postgresql_backend::commit_transaction()
{
	if (!is_initialized()) {
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
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult postgresql_backend::rollback_transaction()
{
	if (!is_initialized()) {
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

	if (result.is_err()) {
		return result;
	}

	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::Result<uint64_t> postgresql_backend::execute_batch(
	const std::vector<std::string>& queries)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"postgresql_backend"
		};
	}

	if (queries.empty()) {
		return static_cast<uint64_t>(0);
	}

	// Use existing transaction API for atomicity
	auto begin_result = begin_transaction();
	if (begin_result.is_err()) {
		return kcenon::common::error_info{
			begin_result.error().code,
			"Batch begin failed: " + begin_result.error().message,
			"postgresql_backend"
		};
	}

	uint64_t total_affected = 0;
	for (size_t i = 0; i < queries.size(); ++i) {
		unsigned int affected = execute_modification_query(queries[i]);
		if (!last_error_.empty()) {
			std::string batch_error = "Batch query " + std::to_string(i) +
				" failed: " + last_error_;
			rollback_transaction();
			last_error_ = batch_error;
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"postgresql_backend"
			};
		}
		total_affected += affected;
	}

	auto commit_result = commit_transaction();
	if (commit_result.is_err()) {
		return kcenon::common::error_info{
			commit_result.error().code,
			"Batch commit failed: " + commit_result.error().message,
			"postgresql_backend"
		};
	}

	last_error_.clear();
	return total_affected;
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

std::string postgresql_backend::build_safe_connection_string(const core::connection_config& config) const
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
		oss << "password=*** ";
	}

	for (const auto& [key, value] : config.options) {
		oss << key << "=" << value << " ";
	}

	return oss.str();
}

std::string postgresql_backend::sanitize_error(const std::string& error_message) const
{
	if (connection_config_.password.empty()) {
		return error_message;
	}

	std::string sanitized = error_message;
	std::string::size_type pos = 0;
	while ((pos = sanitized.find(connection_config_.password, pos)) != std::string::npos) {
		sanitized.replace(pos, connection_config_.password.length(), "***");
		pos += 3;
	}

	return sanitized;
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when PostgreSQL support is compiled in
#ifdef USE_POSTGRESQL
namespace {
	database::core::backend_registrar<database::backends::postgresql_backend> postgresql_registrar("postgresql");
}
#endif
