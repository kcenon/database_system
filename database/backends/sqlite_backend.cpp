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

#include "sqlite_backend.h"

#include <variant>

namespace database
{
namespace backends
{

sqlite_backend::sqlite_backend()
	: manager_(std::make_unique<sqlite_manager>())
{
}

sqlite_backend::~sqlite_backend()
{
	shutdown();
}

std::unique_ptr<core::database_backend> sqlite_backend::create()
{
	return std::make_unique<sqlite_backend>();
}

database_types sqlite_backend::type() const
{
	return database_types::sqlite;
}

database::result<void> sqlite_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return database::result<void>::err(database::error_info("Backend already initialized"));
	}

	connection_config_ = config;
	std::string db_path = get_database_path(config);

	if (!manager_->connect(db_path)) {
		last_error_ = "Failed to connect to SQLite database";
		return database::result<void>::err(database::error_info(last_error_));
	}

	initialized_ = true;
	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
}

database::result<void> sqlite_backend::shutdown()
{
	if (!initialized_) {
		return database::result<void>::ok(std::monostate{}); // Already shutdown
	}

	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

	if (!manager_->disconnect()) {
		last_error_ = "Failed to disconnect from SQLite database";
		return database::result<void>::err(database::error_info(last_error_));
	}

	initialized_ = false;
	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
}

bool sqlite_backend::is_initialized() const
{
	return initialized_;
}

database::result<uint64_t> sqlite_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->insert_query(query_string);
	last_error_.clear();
	return database::result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::result<uint64_t> sqlite_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->update_query(query_string);
	last_error_.clear();
	return database::result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::result<uint64_t> sqlite_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->delete_query(query_string);
	last_error_.clear();
	return database::result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::result<database_result> sqlite_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<database_result>::err(database::error_info(last_error_));
	}

	database_result result = manager_->select_query(query_string);
	last_error_.clear();
	return database::result<database_result>::ok(std::move(result));
}

database::result<void> sqlite_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (!manager_->execute_query(query_string)) {
		last_error_ = "Query execution failed";
		return database::result<void>::err(database::error_info(last_error_));
	}

	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
}

database::result<void> sqlite_backend::begin_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (!manager_->execute_query("BEGIN TRANSACTION")) {
		last_error_ = "Failed to begin transaction";
		return database::result<void>::err(database::error_info(last_error_));
	}

	in_transaction_ = true;
	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
}

database::result<void> sqlite_backend::commit_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (!manager_->execute_query("COMMIT")) {
		last_error_ = "Failed to commit transaction";
		return database::result<void>::err(database::error_info(last_error_));
	}

	in_transaction_ = false;
	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
}

database::result<void> sqlite_backend::rollback_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::result<void>::err(database::error_info(last_error_));
	}

	if (!in_transaction_) {
		// Not an error - already rolled back or never started
		return database::result<void>::ok(std::monostate{});
	}

	if (!manager_->execute_query("ROLLBACK")) {
		last_error_ = "Failed to rollback transaction";
		in_transaction_ = false; // Force state reset even on error
		return database::result<void>::err(database::error_info(last_error_));
	}

	in_transaction_ = false;
	last_error_.clear();
	return database::result<void>::ok(std::monostate{});
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

std::string sqlite_backend::get_database_path(const core::connection_config& config) const
{
	// SQLite uses file paths, stored in the database field
	// Support special values like ":memory:" for in-memory databases
	if (!config.database.empty()) {
		return config.database;
	}

	// Default to in-memory database if no path specified
	return ":memory:";
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when SQLite support is compiled in
#ifdef USE_SQLITE
namespace {
	database::core::backend_registrar<database::backends::sqlite_backend> sqlite_registrar("sqlite");
}
#endif
