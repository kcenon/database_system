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

#include <sstream>
#include <iomanip>
#include <variant>

namespace database
{
namespace backends
{

mysql_backend::mysql_backend()
	: manager_(std::make_unique<mysql_manager>())
{
}

mysql_backend::~mysql_backend()
{
	shutdown();
}

std::unique_ptr<core::database_backend> mysql_backend::create()
{
	return std::make_unique<mysql_backend>();
}

database_types mysql_backend::type() const
{
	return database_types::mysql;
}

database::result<void> mysql_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			"Backend already initialized",
			"mysql_backend"
		};
	}

	connection_config_ = config;
	std::string conn_str = build_connection_string(config);

	if (!manager_->connect(conn_str)) {
		last_error_ = "Failed to connect to MySQL server";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mysql_backend"
		};
	}

	initialized_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

database::result<void> mysql_backend::shutdown()
{
	if (!initialized_) {
		return kcenon::common::ok(); // Already shutdown
	}

	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

	if (!manager_->disconnect()) {
		last_error_ = "Failed to disconnect from MySQL server";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mysql_backend"
		};
	}

	initialized_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

bool mysql_backend::is_initialized() const
{
	return initialized_;
}

database::result<uint64_t> mysql_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = manager_->insert_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

database::result<uint64_t> mysql_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = manager_->update_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

database::result<uint64_t> mysql_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	unsigned int affected = manager_->delete_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

database::result<database_result> mysql_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	database_result result = manager_->select_query(query_string);
	last_error_.clear();
	return result;
}

database::result<void> mysql_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mysql_backend"
		};
	}

	if (!manager_->execute_query(query_string)) {
		last_error_ = "Query execution failed";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}

	last_error_.clear();
	return kcenon::common::ok();
}

database::result<void> mysql_backend::begin_transaction()
{
	if (!initialized_) {
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

	if (!manager_->execute_query("START TRANSACTION")) {
		last_error_ = "Failed to begin transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

database::result<void> mysql_backend::commit_transaction()
{
	if (!initialized_) {
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

	if (!manager_->execute_query("COMMIT")) {
		last_error_ = "Failed to commit transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

database::result<void> mysql_backend::rollback_transaction()
{
	if (!initialized_) {
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

	if (!manager_->execute_query("ROLLBACK")) {
		last_error_ = "Failed to rollback transaction";
		in_transaction_ = false; // Force state reset even on error
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mysql_backend"
		};
	}

	in_transaction_ = false;
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

std::string mysql_backend::build_connection_string(const core::connection_config& config) const
{
	std::ostringstream oss;

	if (!config.host.empty()) {
		oss << "host=" << config.host << ";";
	}

	if (config.port > 0) {
		oss << "port=" << config.port << ";";
	}

	if (!config.database.empty()) {
		oss << "database=" << config.database << ";";
	}

	if (!config.username.empty()) {
		oss << "user=" << config.username << ";";
	}

	if (!config.password.empty()) {
		oss << "password=" << config.password << ";";
	}

	// Append any additional options
	for (const auto& [key, value] : config.options) {
		oss << key << "=" << value << ";";
	}

	return oss.str();
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when MySQL support is compiled in
#ifdef USE_MYSQL
namespace {
	database::core::backend_registrar<database::backends::mysql_backend> mysql_registrar("mysql");
}
#endif
