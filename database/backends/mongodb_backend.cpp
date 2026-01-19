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
#include "../core/result.h"

#ifdef USE_MONGODB
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/view.hpp>
#endif

#include <sstream>
#include <variant>
#include <iostream>
#include <vector>

// Logging helper macros
#define MONGODB_LOG_ERROR(context, message) \
	std::cerr << "[MongoDB:" << context << "] Error: " << message << std::endl
#define MONGODB_LOG_WARNING(message) \
	std::cerr << "[MongoDB] Warning: " << message << std::endl
#define MONGODB_LOG_INFO(message) \
	std::cout << "[MongoDB] Info: " << message << std::endl

namespace database
{
namespace backends
{

mongodb_backend::mongodb_backend()
	: client_(nullptr), database_(nullptr)
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

kcenon::common::VoidResult mongodb_backend::initialize(const core::connection_config& config)
{
	if (initialized_) {
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			"Backend already initialized",
			"mongodb_backend"
		};
	}

	connection_config_ = config;
	std::string conn_uri = build_connection_uri(config);
	db_name_ = config.database;

#ifdef USE_MONGODB
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		// Initialize MongoDB instance (should only be done once per application)
		static mongocxx::instance instance{};

		// Create MongoDB URI
		mongocxx::uri uri{conn_uri};

		// Create MongoDB client
		auto client = std::make_unique<mongocxx::client>(uri);
		client_ = client.release();

		// Get database reference
		auto* mongo_client = static_cast<mongocxx::client*>(client_);
		auto db = std::make_unique<mongocxx::database>((*mongo_client)[db_name_]);
		database_ = db.release();

		// Test connection by running a simple command
		auto* mongo_db = static_cast<mongocxx::database*>(database_);
		auto result = mongo_db->run_command(bsoncxx::builder::stream::document{} << "ping" << 1 << bsoncxx::builder::stream::finalize);

		initialized_ = true;
		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		MONGODB_LOG_ERROR("initialize", last_error_);
	}
#else
	MONGODB_LOG_WARNING("MongoDB support not compiled. Mock mode enabled.");
	// Mock mode for testing without MongoDB
	initialized_ = true;
	last_error_.clear();
	return kcenon::common::ok();
#endif

	if (last_error_.empty()) {
		last_error_ = "Failed to connect to MongoDB server";
	}
	return kcenon::common::error_info{
		static_cast<int>(database::error_code::connection_failed),
		last_error_,
		"mongodb_backend"
	};
}

kcenon::common::VoidResult mongodb_backend::shutdown()
{
	if (!initialized_) {
		return kcenon::common::ok(); // Already shutdown
	}

	// Rollback any active transaction before disconnecting
	if (in_transaction_) {
		rollback_transaction();
	}

#ifdef USE_MONGODB
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	if (database_) {
		delete static_cast<mongocxx::database*>(database_);
		database_ = nullptr;
	}
	if (client_) {
		delete static_cast<mongocxx::client*>(client_);
		client_ = nullptr;
	}
#endif

	initialized_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

bool mongodb_backend::is_initialized() const
{
	return initialized_;
}

bool mongodb_backend::parse_query_string(const std::string& query_string,
										  std::string& collection,
										  std::string& filter,
										  std::string& update) const
{
	// Parse format: "collection_name:filter_json" or "collection_name:filter_json:update_json"
	std::istringstream ss(query_string);
	std::string part;
	std::vector<std::string> parts;

	while (std::getline(ss, part, ':')) {
		parts.push_back(part);
	}

	if (parts.size() >= 1) collection = parts[0];
	if (parts.size() >= 2) filter = parts[1];
	if (parts.size() >= 3) update = parts[2];

	return !collection.empty();
}

kcenon::common::Result<uint64_t> mongodb_backend::insert_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

#ifdef USE_MONGODB
	if (!database_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mongodb_backend"
		};
	}
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		std::string collection_name, document_json, filter;
		if (!parse_query_string(query_string, collection_name, document_json, filter)) {
			last_error_ = "Invalid query format";
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"mongodb_backend"
			};
		}

		auto* mongo_db = static_cast<mongocxx::database*>(database_);
		auto collection = (*mongo_db)[collection_name];

		// Parse JSON to BSON
		auto doc = bsoncxx::from_json(document_json);
		auto result = collection.insert_one(doc.view());

		last_error_.clear();
		return result ? uint64_t(1) : uint64_t(0);
	} catch (const std::exception& e) {
		last_error_ = std::string("Insert error: ") + e.what();
		MONGODB_LOG_ERROR("insert_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	MONGODB_LOG_WARNING("MongoDB support not compiled. Insert query: " + query_string.substr(0, 20) + "...");
	return uint64_t(1); // Mock: return 1 inserted
#endif
}

kcenon::common::Result<uint64_t> mongodb_backend::update_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

#ifdef USE_MONGODB
	if (!database_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mongodb_backend"
		};
	}
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		std::string collection_name, filter_json, update_json;
		if (!parse_query_string(query_string, collection_name, filter_json, update_json)) {
			last_error_ = "Invalid query format";
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"mongodb_backend"
			};
		}

		auto* mongo_db = static_cast<mongocxx::database*>(database_);
		auto collection = (*mongo_db)[collection_name];

		// Parse JSONs to BSON
		auto filter = bsoncxx::from_json(filter_json);
		auto update = bsoncxx::from_json(update_json);

		auto result = collection.update_many(filter.view(), update.view());
		last_error_.clear();
		return result ? static_cast<uint64_t>(result->modified_count()) : uint64_t(0);
	} catch (const std::exception& e) {
		last_error_ = std::string("Update error: ") + e.what();
		MONGODB_LOG_ERROR("update_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	MONGODB_LOG_WARNING("MongoDB support not compiled. Update query: " + query_string.substr(0, 20) + "...");
	return uint64_t(1); // Mock: return 1 updated
#endif
}

kcenon::common::Result<uint64_t> mongodb_backend::delete_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

#ifdef USE_MONGODB
	if (!database_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mongodb_backend"
		};
	}
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		std::string collection_name, filter_json, unused;
		if (!parse_query_string(query_string, collection_name, filter_json, unused)) {
			last_error_ = "Invalid query format";
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"mongodb_backend"
			};
		}

		auto* mongo_db = static_cast<mongocxx::database*>(database_);
		auto collection = (*mongo_db)[collection_name];

		// Parse JSON to BSON
		auto filter = bsoncxx::from_json(filter_json);
		auto result = collection.delete_many(filter.view());

		last_error_.clear();
		return result ? static_cast<uint64_t>(result->deleted_count()) : uint64_t(0);
	} catch (const std::exception& e) {
		last_error_ = std::string("Delete error: ") + e.what();
		MONGODB_LOG_ERROR("delete_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	MONGODB_LOG_WARNING("MongoDB support not compiled. Delete query: " + query_string.substr(0, 20) + "...");
	return uint64_t(1); // Mock: return 1 deleted
#endif
}

kcenon::common::Result<core::database_result> mongodb_backend::select_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	core::database_result result;

#ifdef USE_MONGODB
	if (!database_) {
		last_error_ = "No active connection";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mongodb_backend"
		};
	}
	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		std::string collection_name, filter_json, unused;
		if (!parse_query_string(query_string, collection_name, filter_json, unused)) {
			last_error_ = "Invalid query format";
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::query_failed),
				last_error_,
				"mongodb_backend"
			};
		}

		auto* mongo_db = static_cast<mongocxx::database*>(database_);
		auto collection = (*mongo_db)[collection_name];

		// Parse JSON to BSON for filter
		bsoncxx::document::value filter_doc = filter_json.empty() ?
			bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize :
			bsoncxx::from_json(filter_json);

		auto cursor = collection.find(filter_doc.view());

		for (auto&& doc : cursor) {
			core::database_row row;

			// Convert BSON document to core::database_row
			auto json_string = bsoncxx::to_json(doc);
			row["_document"] = json_string;  // Store full document as JSON string

			// Also extract common fields
			auto view = doc.view();
			if (view["_id"]) {
				row["_id"] = bsoncxx::to_json(view["_id"].get_value());
			}

			// Extract other fields as strings for compatibility
			for (auto&& element : view) {
				std::string key = element.key().to_string();
				if (key != "_id") {  // _id already processed
					row[key] = bsoncxx::to_json(element.get_value());
				}
			}

			result.push_back(std::move(row));
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Select error: ") + e.what();
		MONGODB_LOG_ERROR("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	MONGODB_LOG_WARNING("MongoDB support not compiled. Select query: " + query_string.substr(0, 20) + "...");
	// Return mock data for testing
	core::database_row mock_row;
	mock_row["_id"] = std::string("mock_object_id");
	mock_row["name"] = std::string("mongodb_mock_data");
	mock_row["_document"] = std::string("{\"_id\":\"mock_object_id\",\"name\":\"mongodb_mock_data\"}");
	result.push_back(mock_row);
#endif

	last_error_.clear();
	return result;
}

kcenon::common::VoidResult mongodb_backend::execute_query(const std::string& query_string)
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

#ifdef USE_MONGODB
	if (!database_) {
		last_error_ = "No active MongoDB connection";
		MONGODB_LOG_ERROR("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::connection_failed),
			last_error_,
			"mongodb_backend"
		};
	}

	std::lock_guard<std::mutex> lock(mongo_mutex_);
	try {
		auto* mongo_db = static_cast<mongocxx::database*>(database_);

		// Try to parse as MongoDB command (JSON format)
		try {
			auto command_doc = bsoncxx::from_json(query_string);
			auto result = mongo_db->run_command(command_doc.view());
			last_error_.clear();
			return kcenon::common::ok();
		} catch (const std::exception&) {
			// If JSON parsing fails, treat as collection creation
			std::string collection_name = query_string;
			auto collection = (*mongo_db)[collection_name];
			auto doc = bsoncxx::builder::stream::document{} << "test" << "creation" << bsoncxx::builder::stream::finalize;
			collection.insert_one(doc.view());
			collection.delete_one(doc.view());
			last_error_.clear();
			return kcenon::common::ok();
		}
	} catch (const std::exception& e) {
		last_error_ = std::string("Execute error: ") + e.what();
		MONGODB_LOG_ERROR("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	// Mock execution
	MONGODB_LOG_INFO("MongoDB support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::VoidResult mongodb_backend::begin_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	if (in_transaction_) {
		last_error_ = "Transaction already active";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	// MongoDB transactions require replica sets or sharded clusters
	// For simplicity, we'll mark the transaction state
	in_transaction_ = true;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult mongodb_backend::commit_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	if (!in_transaction_) {
		last_error_ = "No active transaction";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
}

kcenon::common::VoidResult mongodb_backend::rollback_transaction()
{
	if (!initialized_) {
		last_error_ = "Backend not initialized";
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::invalid_state),
			last_error_,
			"mongodb_backend"
		};
	}

	if (!in_transaction_) {
		// Not an error - already rolled back or never started
		return kcenon::common::ok();
	}

	in_transaction_ = false;
	last_error_.clear();
	return kcenon::common::ok();
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
