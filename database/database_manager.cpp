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

#include "database/postgres_manager.h"
#include "database/backends/mysql/mysql_manager.h"
#include "database/backends/sqlite/sqlite_manager.h"
#include "database/backends/mongodb/mongodb_manager.h"
#include "database/backends/redis/redis_manager.h"
#include "database/proxy/proxy_connector.h"

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

		switch (database_type)
		{
		case database_types::postgres:
			database_ = std::make_unique<postgres_manager>();
			break;
		case database_types::mysql:
			database_ = std::make_unique<mysql_manager>();
			break;
		case database_types::sqlite:
			database_ = std::make_unique<sqlite_manager>();
			break;
		case database_types::mongodb:
			database_ = std::make_unique<mongodb_manager>();
			break;
		case database_types::redis:
			database_ = std::make_unique<redis_manager>();
			break;
		default:
			break;
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

		return database_->database_type();
	}

	bool database_manager::connect(const std::string& connect_string)
	{
		if (!database_)
		{
			return false;
		}

		connected_ = database_->connect(connect_string);
		return connected_;
	}

	bool database_manager::create_query(const std::string& query_string)
	{
		if (!database_)
		{
			return false;
		}

		return database_->create_query(query_string);
	}

	unsigned int database_manager::insert_query(const std::string& query_string)
	{
		if (!database_)
		{
			return 0;
		}

		return database_->insert_query(query_string);
	}

	unsigned int database_manager::update_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return 0;
		}

		return database_->update_query(query_string);
	}

	unsigned int database_manager::delete_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return 0;
		}

		return database_->delete_query(query_string);
	}

	database_result database_manager::select_query(const std::string& query_string)
	{
		if (database_ == nullptr)
		{
			return database_result{};
		}

		return database_->select_query(query_string);
	}

	bool database_manager::disconnect(void)
	{
		if (database_ == nullptr)
		{
			return false;
		}

		bool result = database_->disconnect();
		if (result)
		{
			connected_ = false;
		}
		return result;
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
		return kcenon::common::VoidResult(
			kcenon::common::error_info{-1, "Failed to prepare database query", "database_system"});
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