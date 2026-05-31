// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/database_manager.h>

#include <kcenon/database/core/backend_registry.h>
#include <kcenon/database/backends/postgresql_backend.h>
#include <kcenon/database/backends/sqlite_backend.h>
#ifdef USE_MONGODB
#include <kcenon/database/backends/mongodb_backend.h>
#endif
#ifdef USE_REDIS
#include <kcenon/database/backends/redis_backend.h>
#endif
#include <sstream>

namespace kcenon::database
{
	database_manager::database_manager(std::shared_ptr<database_context> context)
		: connected_(false)
		, database_(nullptr)
		, context_(std::move(context))
	{
		// DI constructor - recommended for new code
		if (!context_)
		{
			// Fallback to default context if nullptr passed
			context_ = std::make_shared<database_context>();
		}
	}

	database_manager::~database_manager() {}

	bool database_manager::set_mode(const database_types& database_type)
	{
		if (connected_)
		{
			return false;
		}

		database_.reset();

		// Create the appropriate backend directly
		// This ensures backends are linked even when static library registration
		// might not work due to initialization order
		switch (database_type)
		{
		case database_types::postgres:
			database_ = backends::postgresql_backend::create();
			break;
		case database_types::sqlite:
			database_ = backends::sqlite_backend::create();
			break;
#ifdef USE_MONGODB
		case database_types::mongodb:
			database_ = backends::mongodb_backend::create();
			break;
#endif
#ifdef USE_REDIS
		case database_types::redis:
			database_ = backends::redis_backend::create();
			break;
#endif
		default:
			return false;
		}

		if (database_ == nullptr)
		{
			return false;
		}

		return true;
	}

	database_types database_manager::database_type(void)
	{
		if (!database_)
		{
			return database_types::none;
		}

		return database_->type();
	}

	kcenon::common::VoidResult database_manager::connect_result(const std::string& connect_string)
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend configured", "database_manager"});
		}

		// Store connection string for potential reconnection
		connect_string_ = connect_string;

		// Use database_backend's initialize method with connection_config
		auto config = core::connection_config::from_string(connect_string);
		auto result = database_->initialize(config);

		if (result.is_ok())
		{
			connected_ = true;
		}
		return result;
	}

	kcenon::common::VoidResult database_manager::disconnect_result()
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}

		auto result = database_->shutdown();
		if (result.is_ok())
		{
			connected_ = false;
		}
		return result;
	}

	kcenon::common::VoidResult database_manager::create_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		// database_backend uses execute_query for DDL/prepared statements
		return database_->execute_query(query_string);
	}

	kcenon::common::Result<core::database_result> database_manager::select_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::Result<core::database_result>(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->select_query(query_string);
	}

	kcenon::common::VoidResult database_manager::execute_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->execute_query(query_string);
	}

	kcenon::common::VoidResult database_manager::begin_transaction()
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->begin_transaction();
	}

	kcenon::common::VoidResult database_manager::commit_transaction()
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->commit_transaction();
	}

	kcenon::common::VoidResult database_manager::rollback_transaction()
	{
		if (!database_)
		{
			return kcenon::common::VoidResult(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->rollback_transaction();
	}

	bool database_manager::in_transaction() const
	{
		if (!database_)
		{
			return false;
		}
		return database_->in_transaction();
	}

	std::string database_manager::last_error() const
	{
		if (!database_)
		{
			return "No database backend";
		}
		return database_->last_error();
	}

	std::map<std::string, std::string> database_manager::connection_info() const
	{
		if (!database_)
		{
			return {};
		}
		return database_->connection_info();
	}

	// Connection pool methods moved to header as inline functions for performance

	query_builder database_manager::create_query_builder()
	{
		return query_builder(database_type());
	}

	query_builder database_manager::create_query_builder(database_types db_type)
	{
		return query_builder(db_type);
	}

}; // namespace kcenon::database