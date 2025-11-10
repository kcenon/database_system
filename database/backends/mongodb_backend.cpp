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

#include "mongodb_backend.h"

#include <sstream>
#include <variant>

namespace database
{
namespace backends
{

mongodb_backend::mongodb_backend()
	: manager_(std::make_unique<mongodb_manager>())
{
}

mongodb_backend::~mongodb_backend()
{
	shutdown();
}

std::unique_ptr<core::database_backend> mongodb_backend::create()
{
	return std::make_unique<mongodb_backend>();
}

database_types mongodb_backend::type() const
{
	return database_types::mongodb;
}

database::VoidResult mongodb_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return database::VoidResult::err(database::error_info("Backend already initialized"));
	}

	connection_config_ = config;
	std::string conn_uri = build_connection_uri(config);

	if (!manager_->connect(conn_uri)) {
		last_error_ = "Failed to connect to MongoDB server";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	initialized_ = true;
	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

database::VoidResult mongodb_backend::shutdown()
{
	if (!initialized_) {
		return database::VoidResult::ok(std::monostate{}); // Already shutdown
	}

	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

	if (!manager_->disconnect()) {
		last_error_ = "Failed to disconnect from MongoDB server";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	initialized_ = false;
	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

bool mongodb_backend::is_initialized() const
{
	return initialized_;
}

database::Result<uint64_t> mongodb_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::Result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->insert_query(query_string);
	last_error_.clear();
	return database::Result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::Result<uint64_t> mongodb_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::Result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->update_query(query_string);
	last_error_.clear();
	return database::Result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::Result<uint64_t> mongodb_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::Result<uint64_t>::err(database::error_info(last_error_));
	}

	unsigned int affected = manager_->delete_query(query_string);
	last_error_.clear();
	return database::Result<uint64_t>::ok(static_cast<uint64_t>(affected));
}

database::Result<database_result> mongodb_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::Result<database_result>::err(database::error_info(last_error_));
	}

	database_result result = manager_->select_query(query_string);
	last_error_.clear();
	return database::Result<database_result>::ok(std::move(result));
}

database::VoidResult mongodb_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	if (!manager_->execute_query(query_string)) {
		last_error_ = "Query execution failed";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

database::VoidResult mongodb_backend::begin_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	// MongoDB transactions require replica sets or sharded clusters
	// For simplicity, we'll mark the transaction state
	// The actual transaction implementation is handled by mongodb_manager
	in_transaction_ = true;
	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

database::VoidResult mongodb_backend::commit_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	in_transaction_ = false;
	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

database::VoidResult mongodb_backend::rollback_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return database::VoidResult::err(database::error_info(last_error_));
	}

	if (!in_transaction_) {
		// Not an error - already rolled back or never started
		return database::VoidResult::ok(std::monostate{});
	}

	in_transaction_ = false;
	last_error_.clear();
	return database::VoidResult::ok(std::monostate{});
}

bool mongodb_backend::in_transaction() const
{
	return in_transaction_;
}

std::string mongodb_backend::last_error() const
{
	return last_error_;
}

std::map<std::string, std::string> mongodb_backend::connection_info() const
{
	std::map<std::string, std::string> info;
	info["backend"] = "mongodb";
	info["host"] = connection_config_.host;
	info["port"] = std::to_string(connection_config_.port);
	info["database"] = connection_config_.database;
	info["username"] = connection_config_.username;
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

std::string mongodb_backend::build_connection_uri(const core::connection_config& config) const
{
	std::ostringstream oss;

	oss << "mongodb://";

	// Add credentials if provided
	if (!config.username.empty()) {
		oss << config.username;
		if (!config.password.empty()) {
			oss << ":" << config.password;
		}
		oss << "@";
	}

	// Add host (default to localhost if not specified)
	if (!config.host.empty()) {
		oss << config.host;
	} else {
		oss << "localhost";
	}

	// Add port (default to 27017 if not specified)
	if (config.port > 0) {
		oss << ":" << config.port;
	} else {
		oss << ":27017";
	}

	// Add database
	if (!config.database.empty()) {
		oss << "/" << config.database;
	}

	// Add additional options
	if (!config.options.empty()) {
		oss << "?";
		bool first = true;
		for (const auto& [key, value] : config.options) {
			if (!first) {
				oss << "&";
			}
			oss << key << "=" << value;
			first = false;
		}
	}

	return oss.str();
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when MongoDB support is compiled in
#ifdef USE_MONGODB
namespace {
	database::core::backend_registrar<database::backends::mongodb_backend> mongodb_registrar("mongodb");
}
#endif
