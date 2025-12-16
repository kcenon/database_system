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

#include "redis_backend.h"
#include "../core/result.h"

#include <sstream>
#include <variant>

namespace database
{
namespace backends
{

redis_backend::redis_backend()
	: manager_(std::make_unique<redis_manager>())
{
}

redis_backend::~redis_backend()
{
	shutdown();
}

std::unique_ptr<core::database_backend> redis_backend::create()
{
	return std::make_unique<redis_backend>();
}

database_types redis_backend::type() const
{
	return database_types::redis;
}

kcenon::common::VoidResult redis_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			"Backend already initialized",
			"redis_backend"
		};
	}

	connection_config_ = config;
	std::string conn_str = build_connection_string(config);

	if (!manager_->connect(conn_str)) {
		last_error_ = "Failed to connect to Redis server";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"redis_backend"
		};
	}

	initialized_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::shutdown()
{
	if (!initialized_) {
		return kcenon::common::ok(); // Already shutdown
	}

	// Discard any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

	if (!manager_->disconnect()) {
		last_error_ = "Failed to disconnect from Redis server";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"redis_backend"
		};
	}

	initialized_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

bool redis_backend::is_initialized() const
{
	return initialized_;
}

kcenon::common::Result<uint64_t> redis_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	unsigned int affected = manager_->insert_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> redis_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	unsigned int affected = manager_->update_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<uint64_t> redis_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	unsigned int affected = manager_->delete_query(query_string);
	last_error_.clear();
	return static_cast<uint64_t>(affected);
}

kcenon::common::Result<database_result> redis_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	database_result result = manager_->select_query(query_string);
	last_error_.clear();
	return result;
}

kcenon::common::VoidResult redis_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	if (!manager_->execute_query(query_string)) {
		last_error_ = "Query execution failed";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}

	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::begin_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	// Redis uses MULTI to begin a transaction
	if (!manager_->execute_query("MULTI")) {
		last_error_ = "Failed to begin transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::commit_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	// Redis uses EXEC to commit a transaction
	if (!manager_->execute_query("EXEC")) {
		last_error_ = "Failed to commit transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::rollback_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	if (!in_transaction_) {
		// Not an error - already discarded or never started
		return kcenon::common::ok();
	}

	// Redis uses DISCARD to rollback a transaction
	if (!manager_->execute_query("DISCARD")) {
		last_error_ = "Failed to rollback transaction";
		in_transaction_ = false; // Force state reset even on error
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

bool redis_backend::in_transaction() const
{
	return in_transaction_;
}

std::string redis_backend::last_error() const
{
	return last_error_;
}

std::map<std::string, std::string> redis_backend::connection_info() const
{
	std::map<std::string, std::string> info;
	info["backend"] = "redis";
	info["host"] = connection_config_.host;
	info["port"] = std::to_string(connection_config_.port);
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

std::string redis_backend::build_connection_string(const core::connection_config& config) const
{
	std::ostringstream oss;

	// Redis connection string format: "host:port" or "redis://host:port"
	if (!config.host.empty()) {
		oss << config.host;
	} else {
		oss << "localhost";
	}

	oss << ":";

	if (config.port > 0) {
		oss << config.port;
	} else {
		oss << "6379"; // Default Redis port
	}

	return oss.str();
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when Redis support is compiled in
#ifdef USE_REDIS
namespace {
	database::core::backend_registrar<database::backends::redis_backend> redis_registrar("redis");
}
#endif
