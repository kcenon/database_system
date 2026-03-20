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

#ifdef USE_REDIS
#include <hiredis/hiredis.h>
#endif

#include <sstream>
#include <variant>
#include <iostream>
#include <vector>

#include "../utils/backend_logger.h"

namespace
{
const database::utils::backend_logger logger_("Redis");
}

namespace database
{
namespace backends
{

redis_backend::redis_backend()
	: context_(nullptr), host_("localhost"), port_(6379)
{
}

kcenon::common::VoidResult redis_backend::do_initialize(const core::connection_config& config)
{
	connection_config_ = config;
	host_ = config.host.empty() ? "localhost" : config.host;
	port_ = config.port > 0 ? config.port : 6379;

#ifdef USE_REDIS
	std::lock_guard<std::mutex> lock(redis_mutex_);
	try {
		// Create Redis connection
		redisContext* ctx = redisConnect(host_.c_str(), port_);
		if (ctx == nullptr || ctx->err) {
			if (ctx) {
				last_error_ = std::string("Connection error: ") + ctx->errstr;
				logger_.error("do_initialize", last_error_);
				redisFree(ctx);
			} else {
				last_error_ = "Connection allocation error";
				logger_.error("do_initialize", last_error_);
			}
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"redis_backend"
			};
		}

		context_ = ctx;

		// Authenticate if password provided
		if (!config.password.empty()) {
			redisReply* reply = static_cast<redisReply*>(
				redisCommand(ctx, "AUTH %s", config.password.c_str()));
			if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
				last_error_ = "Authentication failed";
				logger_.error("do_initialize", last_error_);
				if (reply) freeReplyObject(reply);
				redisFree(ctx);
				context_ = nullptr;
				return kcenon::common::error_info{
					static_cast<int>(database::error_code::connection_failed),
					last_error_,
					"redis_backend"
				};
			}
			freeReplyObject(reply);
		}

		// Test connection with PING
		redisReply* ping_reply = static_cast<redisReply*>(redisCommand(ctx, "PING"));
		if (ping_reply == nullptr || ping_reply->type == REDIS_REPLY_ERROR) {
			last_error_ = "PING failed";
			logger_.error("do_initialize", last_error_);
			if (ping_reply) freeReplyObject(ping_reply);
			redisFree(ctx);
			context_ = nullptr;
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::connection_failed),
				last_error_,
				"redis_backend"
			};
		}
		freeReplyObject(ping_reply);

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		logger_.error("do_initialize", last_error_);
	}
#else
	logger_.warning("Redis support not compiled. Mock mode enabled.");
	// Mock mode for testing without Redis
	last_error_.clear();
	return kcenon::common::ok();
#endif

	if (last_error_.empty()) {
		last_error_ = "Failed to connect to Redis server";
	}
	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"redis_backend"
	};
}

kcenon::common::VoidResult redis_backend::do_shutdown()
{
	// Discard any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_REDIS
	std::lock_guard<std::mutex> lock(redis_mutex_);
	if (context_) {
		redisFree(static_cast<redisContext*>(context_));
		context_ = nullptr;
	}
#endif

	last_error_.clear();
	return kcenon::common::ok();
}

bool redis_backend::parse_redis_query(const std::string& query_string,
									   std::string& key,
									   std::string& value) const
{
	// Parse format: "key:value"
	std::istringstream ss(query_string);
	std::string part;
	std::vector<std::string> parts;

	while (std::getline(ss, part, ':')) {
		parts.push_back(part);
	}

	if (parts.size() >= 1) key = parts[0];
	if (parts.size() >= 2) value = parts[1];

	return !key.empty();
}

kcenon::common::Result<core::database_result> redis_backend::select_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

	core::database_result result;

#ifdef USE_REDIS
	if (!context_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"redis_backend"
		};
	}
	std::lock_guard<std::mutex> lock(redis_mutex_);
	try {
		// For select, query_string is the key
		std::string key = query_string;

		redisContext* ctx = static_cast<redisContext*>(context_);
		redisReply* reply = static_cast<redisReply*>(
			redisCommand(ctx, "GET %s", key.c_str()));

		if (reply != nullptr && reply->type != REDIS_REPLY_ERROR &&
			reply->type != REDIS_REPLY_NIL) {
			core::database_row row;
			row["key"] = key;

			if (reply->type == REDIS_REPLY_STRING) {
				row["value"] = std::string(reply->str, reply->len);
			} else if (reply->type == REDIS_REPLY_INTEGER) {
				row["value"] = static_cast<int64_t>(reply->integer);
			} else {
				row["value"] = std::string("");
			}

			result.push_back(std::move(row));
		}

		if (reply) freeReplyObject(reply);
	} catch (const std::exception& e) {
		last_error_ = std::string("Select error: ") + e.what();
		logger_.error("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}
#else
	logger_.warning("Redis support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	if (!query_string.empty()) {
		core::database_row mock_row;
		mock_row["key"] = query_string;
		mock_row["value"] = std::string("redis_mock_value");
		result.push_back(mock_row);
	}
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult redis_backend::execute_query(const std::string& query_string)
{
	if (!is_initialized()) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"redis_backend"
		};
	}

#ifdef USE_REDIS
	if (!context_) {
		last_error_ = "No active Redis connection";
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"redis_backend"
		};
	}

	std::lock_guard<std::mutex> lock(redis_mutex_);
	try {
		redisContext* ctx = static_cast<redisContext*>(context_);
		redisReply* reply = static_cast<redisReply*>(
			redisCommand(ctx, "%s", query_string.c_str()));

		if (reply == nullptr) {
			last_error_ = std::string("Command failed: ") + ctx->errstr;
			logger_.error("execute_query", last_error_);
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"redis_backend"
			};
		}

		bool success = true;
		if (reply->type == REDIS_REPLY_ERROR) {
			last_error_ = std::string("Execute error: ") + reply->str;
			logger_.error("execute_query", last_error_);
			success = false;
		}

		freeReplyObject(reply);

		if (!success) {
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"redis_backend"
			};
		}

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Execute error: ") + e.what();
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"redis_backend"
		};
	}
#else
	// Mock execution
	logger_.info("Redis support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::VoidResult redis_backend::begin_transaction()
{
	if (!is_initialized()) {
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
	auto result = execute_query("MULTI");
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::commit_transaction()
{
	if (!is_initialized()) {
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
	auto result = execute_query("EXEC");
	if (result.is_err()) {
		return result;
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult redis_backend::rollback_transaction()
{
	if (!is_initialized()) {
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
	auto result = execute_query("DISCARD");
	in_transaction_ = false; // Force state reset even on error

	if (result.is_err()) {
		return result;
	}

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
	info["host"] = host_;
	info["port"] = std::to_string(port_);
	info["initialized"] = initialized_ ? "true" : "false";
	info["in_transaction"] = in_transaction_ ? "true" : "false";
	return info;
}

} // namespace backends
} // namespace database

// Auto-registration with backend_registry when Redis support is compiled in
#ifdef USE_REDIS
namespace {
	database::core::backend_registrar<database::backends::redis_backend> redis_registrar("redis");
}
#endif
