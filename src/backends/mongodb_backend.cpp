// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/backends/mongodb_backend.h>
#include <kcenon/database/core/result.h>

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

#include <kcenon/database/utils/backend_logger.h>

namespace
{
const database::utils::backend_logger logger_("MongoDB");
}

namespace database
{
namespace backends
{

mongodb_backend::mongodb_backend()
	: client_(nullptr), database_(nullptr)
{
}

kcenon::common::VoidResult mongodb_backend::do_initialize(const core::connection_config& config)
{
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

		last_error_.clear();
		return kcenon::common::ok();
	} catch (const std::exception& e) {
		last_error_ = std::string("Connection error: ") + e.what();
		logger_.error("do_initialize", last_error_);
	}
#else
	logger_.warning("MongoDB support not compiled. Mock mode enabled.");
	// Mock mode for testing without MongoDB
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

kcenon::common::VoidResult mongodb_backend::do_shutdown()
{
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

	last_error_.clear();
	return kcenon::common::ok();
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

kcenon::common::Result<core::database_result> mongodb_backend::select_query(const std::string& query_string)
{
	if (!is_initialized()) {
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
		logger_.error("select_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	logger_.warning("MongoDB support not compiled. Select query: " + query_string.substr(0, 20) + "...");
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
	if (!is_initialized()) {
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
		logger_.error("execute_query", last_error_);
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
		logger_.error("execute_query", last_error_);
		return kcenon::common::error_info{
			static_cast<int>(database::error_code::query_failed),
			last_error_,
			"mongodb_backend"
		};
	}
#else
	// Mock execution
	logger_.info("MongoDB support not compiled. Mock execute: " + query_string);
	last_error_.clear();
	return kcenon::common::ok();
#endif
}

kcenon::common::VoidResult mongodb_backend::begin_transaction()
{
	if (!is_initialized()) {
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
	if (!is_initialized()) {
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
	if (!is_initialized()) {
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
} // namespace kcenon::database

// Auto-registration with backend_registry when MongoDB support is compiled in
#ifdef USE_MONGODB
namespace {
	database::core::backend_registrar<database::backends::mongodb_backend> mongodb_registrar("mongodb");
}
#endif
