// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/backends/sqlite_backend.h>
#include <kcenon/database/core/result.h>

#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

#include <sstream>
#include <iomanip>
#include <variant>
#include <iostream>
#include <vector>

#include <kcenon/database/utils/backend_logger.h>

namespace
{
const database::utils::backend_logger logger_("SQLite");
}

namespace database
{
namespace backends
{

sqlite_backend::sqlite_backend()
	: connection_(nullptr)
{
}

kcenon::common::VoidResult sqlite_backend::do_initialize(const core::connection_config& config)
{
	connection_config_ = config;

	// Use database field as file path, default to ":memory:" for in-memory database
	std::string db_path = config.database.empty() ? ":memory:" : config.database;

#ifdef USE_SQLITE
	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	try {
		sqlite3* db = nullptr;

		// Open or create the database
		int result = sqlite3_open(db_path.c_str(), &db);

		if (result != SQLITE_OK) {
			last_error_ = std::string("Connection failed: ") + sqlite3_errmsg(db);
			logger_.error("do_initialize", last_error_);
			if (db) {
				sqlite3_close(db);
			}
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"sqlite_backend"
			};
		}

		connection_ = db;

		// Enable foreign key constraints
		char* error_msg = nullptr;
		if (sqlite3_exec(db, "PRAGMA foreign_keys = ON", nullptr, nullptr, &error_msg) != SQLITE_OK) {
			logger_.warning(std::string("Failed to enable foreign key constraints: ") + (error_msg ? error_msg : ""));
			if (error_msg) sqlite3_free(error_msg);
		}

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		logger_.error("do_initialize", last_error_);
	}
#else
	logger_.warning("SQLite support not compiled. Mock mode enabled.");
	// Mock mode for testing without SQLite
	last_error_.clear();
	return kcenon::common::ok();
#endif

	if (last_error_.empty()) {
		last_error_ = "Failed to connect to SQLite database";
	}
	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"sqlite_backend"
	};
}

kcenon::common::VoidResult sqlite_backend::do_shutdown()
{
	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_SQLITE
	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	if (connection_) {
		sqlite3* db = static_cast<sqlite3*>(connection_);
		int result = sqlite3_close(db);
		connection_ = nullptr;
		if (result != SQLITE_OK) {
			last_error_ = "Failed to close SQLite database";
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"sqlite_backend"
			};
		}
	}
#endif

	last_error_.clear();
	return kcenon::common::ok();
}

core::database_value sqlite_backend::convert_sqlite_value(void* stmt, int column_index)
{
#ifdef USE_SQLITE
	sqlite3_stmt* sqlite_stmt = static_cast<sqlite3_stmt*>(stmt);

	int sqlite_type = sqlite3_column_type(sqlite_stmt, column_index);

	switch (sqlite_type) {
		case SQLITE_INTEGER:
			return static_cast<int64_t>(sqlite3_column_int64(sqlite_stmt, column_index));

		case SQLITE_FLOAT:
			return sqlite3_column_double(sqlite_stmt, column_index);

		case SQLITE_TEXT:
			{
				const char* text = reinterpret_cast<const char*>(sqlite3_column_text(sqlite_stmt, column_index));
				return std::string(text ? text : "");
			}

		case SQLITE_BLOB:
			{
				// For BLOB data, convert to string representation
				const void* blob = sqlite3_column_blob(sqlite_stmt, column_index);
				int blob_size = sqlite3_column_bytes(sqlite_stmt, column_index);
				if (blob && blob_size > 0) {
					const char* blob_chars = static_cast<const char*>(blob);
					return std::string(blob_chars, blob_size);
				}
				return std::string();
			}

		case SQLITE_NULL:
		default:
			return nullptr;
	}
#endif
	return nullptr;
}

unsigned int sqlite_backend::execute_modification_query(const std::string& query_string)
{
#ifdef USE_SQLITE
	if (!connection_) return 0;
	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	try {
		sqlite3* db = static_cast<sqlite3*>(connection_);
		char* error_msg = nullptr;

		int result = sqlite3_exec(db, query_string.c_str(), nullptr, nullptr, &error_msg);

		if (result != SQLITE_OK) {
			last_error_ = std::string("Modification query failed: ") + (error_msg ? error_msg : "Unknown error");
			logger_.error("execute_modification_query", last_error_);
			if (error_msg) sqlite3_free(error_msg);
			return 0;
		}

		last_error_.clear();
		return static_cast<unsigned int>(sqlite3_changes(db));
	} catch (const std::exception& e) {
		last_error_ = std::string("Modification query error: ") + e.what();
		logger_.error("execute_modification_query", last_error_);
	}
#else
	logger_.warning("SQLite support not compiled. Modification query: " + query_string.substr(0, 20) + "...");
	return 1; // Mock: return 1 affected row
#endif
	return 0;
}

kcenon::common::Result<core::database_result> sqlite_backend::select_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

	core::database_result result;

#ifdef USE_SQLITE
	if (!connection_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"sqlite_backend"
		};
	}
	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	try {
		sqlite3* db = static_cast<sqlite3*>(connection_);
		sqlite3_stmt* stmt = nullptr;

		// Prepare the statement
		int prepare_result = sqlite3_prepare_v2(db, query_string.c_str(), -1, &stmt, nullptr);
		if (prepare_result != SQLITE_OK) {
			last_error_ = std::string("Prepare failed: ") + sqlite3_errmsg(db);
			logger_.error("select_query", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"sqlite_backend"
			};
		}

		// Get column count and names
		int column_count = sqlite3_column_count(stmt);
		std::vector<std::string> column_names;
		for (int i = 0; i < column_count; i++) {
			column_names.push_back(sqlite3_column_name(stmt, i));
		}

		// Execute and fetch results
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			core::database_row row;

			for (int i = 0; i < column_count; i++) {
				const std::string& column_name = column_names[i];
				row[column_name] = convert_sqlite_value(stmt, i);
			}

			result.push_back(std::move(row));
		}

		// Clean up
		sqlite3_finalize(stmt);

	} catch (const std::exception& e) {
		last_error_ = std::string("Select query error: ") + e.what();
		logger_.error("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}
#else
	logger_.warning("SQLite support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	if (query_string.find("SELECT") != std::string::npos) {
		core::database_row mock_row;
		mock_row["id"] = int64_t(1);
		mock_row["name"] = std::string("sqlite_mock_data");
		mock_row["active"] = true;
		result.push_back(mock_row);
	}
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult sqlite_backend::execute_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

#ifdef USE_SQLITE
	if (!connection_) {
		last_error_ = "No active SQLite connection";
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	sqlite3* db = static_cast<sqlite3*>(connection_);

	char* error_msg = nullptr;
	int result = sqlite3_exec(db, query_string.c_str(), nullptr, nullptr, &error_msg);

	if (result != SQLITE_OK) {
		last_error_ = std::string("Execute error: ") + (error_msg ? error_msg : "Unknown error");
		logger_.error("execute_query", last_error_);
		if (error_msg) sqlite3_free(error_msg);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	last_error_.clear();
	return kcenon::common::ok();
#else
	// Mock execution
	logger_.info("SQLite support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::Result<core::database_result> sqlite_backend::select_prepared(
	const std::string& query,
	const std::vector<core::database_value>& params)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

	core::database_result result;

#ifdef USE_SQLITE
	if (!connection_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	sqlite3* db = static_cast<sqlite3*>(connection_);
	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		last_error_ = std::string("Prepare error: ") + sqlite3_errmsg(db);
		logger_.error("select_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	// Bind parameters
	for (size_t i = 0; i < params.size(); ++i) {
		int bind_idx = static_cast<int>(i + 1); // SQLite uses 1-based indexing
		int bind_rc = SQLITE_OK;

		std::visit([&bind_rc, stmt, bind_idx](const auto& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::nullptr_t>) {
				bind_rc = sqlite3_bind_null(stmt, bind_idx);
			} else if constexpr (std::is_same_v<T, bool>) {
				bind_rc = sqlite3_bind_int(stmt, bind_idx, v ? 1 : 0);
			} else if constexpr (std::is_same_v<T, int64_t>) {
				bind_rc = sqlite3_bind_int64(stmt, bind_idx, v);
			} else if constexpr (std::is_same_v<T, double>) {
				bind_rc = sqlite3_bind_double(stmt, bind_idx, v);
			} else if constexpr (std::is_same_v<T, std::string>) {
				bind_rc = sqlite3_bind_text(stmt, bind_idx, v.c_str(),
					static_cast<int>(v.size()), SQLITE_TRANSIENT);
			}
		}, params[i]);

		if (bind_rc != SQLITE_OK) {
			last_error_ = std::string("Bind error at param ") + std::to_string(i + 1)
				+ ": " + sqlite3_errmsg(db);
			logger_.error("select_prepared", last_error_);
			sqlite3_finalize(stmt);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"sqlite_backend"
			};
		}
	}

	// Execute and collect results
	int col_count = sqlite3_column_count(stmt);
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		core::database_row db_row;
		for (int col = 0; col < col_count; ++col) {
			std::string column_name = sqlite3_column_name(stmt, col);
			db_row[column_name] = convert_sqlite_value(stmt, col);
		}
		result.push_back(std::move(db_row));
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		last_error_ = std::string("Step error: ") + sqlite3_errmsg(db);
		logger_.error("select_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}
#else
	return database_backend::select_prepared(query, params);
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult sqlite_backend::execute_prepared(
	const std::string& query,
	const std::vector<core::database_value>& params)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

#ifdef USE_SQLITE
	if (!connection_) {
		last_error_ = "No active SQLite connection";
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	std::lock_guard<std::recursive_mutex> lock(sqlite_mutex_);
	sqlite3* db = static_cast<sqlite3*>(connection_);
	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		last_error_ = std::string("Prepare error: ") + sqlite3_errmsg(db);
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	for (size_t i = 0; i < params.size(); ++i) {
		int bind_idx = static_cast<int>(i + 1);
		int bind_rc = SQLITE_OK;

		std::visit([&bind_rc, stmt, bind_idx](const auto& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::nullptr_t>) {
				bind_rc = sqlite3_bind_null(stmt, bind_idx);
			} else if constexpr (std::is_same_v<T, bool>) {
				bind_rc = sqlite3_bind_int(stmt, bind_idx, v ? 1 : 0);
			} else if constexpr (std::is_same_v<T, int64_t>) {
				bind_rc = sqlite3_bind_int64(stmt, bind_idx, v);
			} else if constexpr (std::is_same_v<T, double>) {
				bind_rc = sqlite3_bind_double(stmt, bind_idx, v);
			} else if constexpr (std::is_same_v<T, std::string>) {
				bind_rc = sqlite3_bind_text(stmt, bind_idx, v.c_str(),
					static_cast<int>(v.size()), SQLITE_TRANSIENT);
			}
		}, params[i]);

		if (bind_rc != SQLITE_OK) {
			last_error_ = std::string("Bind error at param ") + std::to_string(i + 1)
				+ ": " + sqlite3_errmsg(db);
			logger_.error("execute_prepared", last_error_);
			sqlite3_finalize(stmt);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"sqlite_backend"
			};
		}
	}

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
		last_error_ = std::string("Execute prepared error: ") + sqlite3_errmsg(db);
		logger_.error("execute_prepared", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"sqlite_backend"
		};
	}

	last_error_.clear();
	return kcenon::common::ok();
#else
	return database_backend::execute_prepared(query, params);
#endif
}

kcenon::common::VoidResult sqlite_backend::begin_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

	auto result = execute_query("BEGIN TRANSACTION");
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult sqlite_backend::commit_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
		};
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
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

kcenon::common::VoidResult sqlite_backend::rollback_transaction()
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"sqlite_backend"
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

bool sqlite_backend::in_transaction() const
{
	return in_transaction_;
}

std::string sqlite_backend::last_error() const
{
	return last_error_;
}

std::map<std::string, std::string> sqlite_backend::connection_info() const
{
	std::map<std::string, std::string> info;
	info["backend"] = "sqlite";
	info["database"] = connection_config_.database;
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

} // namespace backends
} // namespace kcenon::database

// Auto-registration with backend_registry when SQLite support is compiled in
#ifdef USE_SQLITE
namespace {
	database::core::backend_registrar<database::backends::sqlite_backend> sqlite_registrar("sqlite");
}
#endif
