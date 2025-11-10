// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "system_logger_backend.h"

#include <kcenon/logger/core/logger.h>
#include <kcenon/logger/interfaces/logger_types.h>
#include <kcenon/logger/writers/console_writer.h>
#include <kcenon/logger/writers/file_writer.h>

#include <stdexcept>
#include <string>

namespace
{
	inline common::VoidResult make_error(const std::string& msg, int code = -1)
	{
		return common::VoidResult(common::error_info{ code, msg, "" });
	}
}

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

system_logger_backend::system_logger_backend(const db_logger_config& config)
	: config_(config), initialized_(false), logger_(nullptr)
{
}

system_logger_backend::~system_logger_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult system_logger_backend::initialize()
{
	if (initialized_)
	{
		return common::ok();
	}

	try
	{
		// Create logger with async support and buffer size
		logger_ = std::make_unique<kcenon::logger::logger>(
			true,  // async mode for better performance
			8192   // buffer size
		);

		// Add console writer
		auto console = std::make_unique<kcenon::logger::console_writer>();
		auto add_console_result = logger_->add_writer(std::move(console));
		if (!add_console_result)
		{
			return make_error("Failed to add console writer");
		}

		// Add file writer if enabled
		if (config_.enable_file_logging)
		{
			std::string log_file = config_.log_directory + "/database.log";
			auto file_writer = std::make_unique<kcenon::logger::file_writer>(log_file);
			auto add_file_result = logger_->add_writer(std::move(file_writer));
			if (!add_file_result)
			{
				return make_error("Failed to add file writer");
			}
		}

		// Set minimum log level
		logger_->set_min_level(convert_log_level(config_.min_log_level));

		// Start the logger
		auto start_result = logger_->start();
		if (!start_result)
		{
			return make_error("Failed to start logger");
		}

		initialized_ = true;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error(std::string("Logger initialization failed: ") + e.what());
	}
}

common::VoidResult system_logger_backend::shutdown()
{
	if (!initialized_)
	{
		return common::ok();
	}

	try
	{
		if (logger_)
		{
			flush();
			auto stop_result = logger_->stop();
			if (!stop_result)
			{
				return make_error("Failed to stop logger");
			}
			logger_.reset();
		}
		initialized_ = false;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error(std::string("Logger shutdown failed: ") + e.what());
	}
}

bool system_logger_backend::is_initialized() const
{
	return initialized_;
}

void system_logger_backend::log(db_log_level level, const std::string& message)
{
	if (!initialized_ || !logger_)
	{
		return;
	}

	// Check if this level should be logged
	if (level < config_.min_log_level)
	{
		return;
	}

	logger_->log(convert_log_level(level), message);
}

void system_logger_backend::flush()
{
	if (logger_)
	{
		logger_->flush();
	}
}

logger_system::log_level system_logger_backend::convert_log_level(db_log_level level)
{
	switch (level)
	{
		case db_log_level::trace:
			return logger_system::log_level::trace;
		case db_log_level::debug:
			return logger_system::log_level::debug;
		case db_log_level::info:
			return logger_system::log_level::info;
		case db_log_level::warning:
			return logger_system::log_level::warning;
		case db_log_level::error:
			return logger_system::log_level::error;
		case db_log_level::critical:
			return logger_system::log_level::critical;
		case db_log_level::fatal:
			// logger_system doesn't have fatal, map to critical
			return logger_system::log_level::critical;
		default:
			return logger_system::log_level::info;
	}
}

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
