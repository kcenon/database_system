/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "database/database_manager.h"

#include "database/core/backend_registry.h"
#include "database/backends/postgresql_backend.h"
#include "database/backends/mysql_backend.h"
#include "database/backends/sqlite_backend.h"
#include "database/backends/mongodb_backend.h"
#include "database/backends/redis_backend.h"
#include "database/proxy/proxy_connector.h"

#include <sstream>

namespace database
{
	database_manager::database_manager(std::shared_ptr<database_context> context)
		: connected_(false)
		, database_(nullptr)
		, context_(std::move(context))
		, connection_mode_(connection_mode::direct)
		, proxy_config_()
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
		case database_types::mysql:
			database_ = backends::mysql_backend::create();
			break;
		case database_types::sqlite:
			database_ = backends::sqlite_backend::create();
			break;
		case database_types::mongodb:
			database_ = backends::mongodb_backend::create();
			break;
		case database_types::redis:
			database_ = backends::redis_backend::create();
			break;
		default:
			return false;
		}

		if (database_ == nullptr)
		{
			return false;
		}

		connection_mode_ = connection_mode::direct;
		return true;
	}

	bool database_manager::set_mode_proxy(const database_types& database_type,
										  const proxy::proxy_connection_config& proxy_config)
	{
		if (connected_)
		{
			return false;
		}

		if (!proxy_config.is_valid())
		{
			return false;
		}

		database_.reset();

		// Create proxy connector for the specified database type
		// proxy_connector now implements database_backend interface
		database_ = std::make_unique<proxy::proxy_connector>(database_type, proxy_config);

		if (database_ == nullptr)
		{
			return false;
		}

		connection_mode_ = connection_mode::proxy;
		proxy_config_ = proxy_config;
		return true;
	}

	connection_mode database_manager::current_connection_mode() const noexcept
	{
		return connection_mode_;
	}

	database_types database_manager::database_type(void)
	{
		if (!database_)
		{
			return database_types::none;
		}

		return database_->type();
	}

	bool database_manager::connect(const std::string& connect_string)
	{
		if (!database_)
		{
			return false;
		}

		// Store connection string for potential reconnection
		connect_string_ = connect_string;

		// Use database_backend's initialize method with connection_config
		auto config = core::connection_config::from_string(connect_string);
		auto result = database_->initialize(config);

		connected_ = result.is_ok();
		return connected_;
	}

	bool database_manager::create_query(const std::string& query_string)
	{
		if (!database_)
		{
			return false;
		}

		// database_backend uses execute_query for DDL/prepared statements
		auto result = database_->execute_query(query_string);
		return result.is_ok();
	}

	unsigned int database_manager::insert_query(const std::string& query_string)
	{
		if (!database_)
		{
			return 0;
		}

		auto result = database_->insert_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	unsigned int database_manager::update_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return 0;
		}

		auto result = database_->update_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	unsigned int database_manager::delete_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return 0;
		}

		auto result = database_->delete_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	database_result database_manager::select_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return database_result{};
		}

		auto result = database_->select_query(query_string);
		if (result.is_ok())
		{
			// Convert from core::database_result to database::database_result
			const auto& core_result = result.value();
			database_result legacy_result;
			legacy_result.reserve(core_result.size());

			for (const auto& row : core_result)
			{
				database_row legacy_row;
				for (const auto& [key, value] : row)
				{
					legacy_row[key] = value;
				}
				legacy_result.push_back(std::move(legacy_row));
			}
			return legacy_result;
		}
		return database_result{};
	}

	bool database_manager::disconnect(void)
	{
		if (database_ == nullptr)
		{
			return false;
		}

		auto result = database_->shutdown();
		if (result.is_ok())
		{
			connected_ = false;
		}
		return result.is_ok();
	}

	kcenon::common::VoidResult database_manager::connect_result(const std::string& connect_string)
	{
		if (connect(connect_string))
		{
			return kcenon::common::ok();
		}
		return kcenon::common::VoidResult(
			kcenon::common::error_info{-1, "Failed to connect to database", "database_system"});
	}

	kcenon::common::VoidResult database_manager::disconnect_result()
	{
		if (disconnect())
		{
			return kcenon::common::ok();
		}
		return kcenon::common::VoidResult(
			kcenon::common::error_info{-1, "Failed to disconnect from database", "database_system"});
	}

	kcenon::common::VoidResult database_manager::create_query_result(const std::string& query_string)
	{
		if (create_query(query_string))
		{
			return kcenon::common::ok();
		}
		std::string error_msg = database_ ? database_->last_error() : "No database backend";
		return kcenon::common::VoidResult(
			kcenon::common::error_info{-1, error_msg, "database_manager"});
	}

	kcenon::common::Result<uint64_t> database_manager::insert_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::Result<uint64_t>(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->insert_query(query_string);
	}

	kcenon::common::Result<uint64_t> database_manager::update_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::Result<uint64_t>(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->update_query(query_string);
	}

	kcenon::common::Result<uint64_t> database_manager::delete_query_result(const std::string& query_string)
	{
		if (!database_)
		{
			return kcenon::common::Result<uint64_t>(
				kcenon::common::error_info{-1, "No database backend", "database_manager"});
		}
		return database_->delete_query(query_string);
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

}; // namespace database