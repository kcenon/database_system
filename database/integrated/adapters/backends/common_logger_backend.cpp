// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/backends/common_logger_backend.h>

#if KCENON_HAS_COMMON_SYSTEM
#include <kcenon/common/interfaces/global_logger_registry.h>
#include <kcenon/common/logging/log_macros.h>
#endif

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

common_logger_backend::common_logger_backend(const db_logger_config& config)
	: config_(config), initialized_(false)
{
}

common_logger_backend::~common_logger_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult common_logger_backend::initialize()
{
	if (initialized_)
	{
		return common::ok();
	}

#if KCENON_HAS_COMMON_SYSTEM
	try
	{
		// The GlobalLoggerRegistry is already initialized by common_system.
		// We just need to verify we can access it.
		auto& registry = kcenon::common::interfaces::GlobalLoggerRegistry::instance();

		// Check if a default logger is available
		if (!registry.has_default_logger())
		{
			// No default logger registered, but that's okay.
			// LOG_* macros will use NullLogger as fallback.
		}

		initialized_ = true;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error(std::string("Logger initialization failed: ") + e.what());
	}
#else
	return make_error("common_system not available");
#endif
}

common::VoidResult common_logger_backend::shutdown()
{
	if (!initialized_)
	{
		return common::ok();
	}

	// GlobalLoggerRegistry is a singleton managed by common_system.
	// We don't need to shut it down here.
	initialized_ = false;
	return common::ok();
}

bool common_logger_backend::is_initialized() const
{
	return initialized_;
}

void common_logger_backend::log(db_log_level level, const std::string& message)
{
	if (!initialized_)
	{
		return;
	}

	// Check if this level should be logged
	if (level < config_.min_log_level)
	{
		return;
	}

#if KCENON_HAS_COMMON_SYSTEM
	// Use common_system's logging functions
	auto common_level = convert_log_level(level);
	kcenon::common::logging::log(common_level, message);
#endif
}

void common_logger_backend::flush()
{
#if KCENON_HAS_COMMON_SYSTEM
	if (initialized_)
	{
		kcenon::common::logging::flush();
	}
#endif
}

#if KCENON_HAS_COMMON_SYSTEM
kcenon::common::interfaces::log_level common_logger_backend::convert_log_level(db_log_level level)
{
	switch (level)
	{
		case db_log_level::trace:
			return kcenon::common::interfaces::log_level::trace;
		case db_log_level::debug:
			return kcenon::common::interfaces::log_level::debug;
		case db_log_level::info:
			return kcenon::common::interfaces::log_level::info;
		case db_log_level::warning:
			return kcenon::common::interfaces::log_level::warning;
		case db_log_level::error:
			return kcenon::common::interfaces::log_level::error;
		case db_log_level::critical:
		case db_log_level::fatal:
			return kcenon::common::interfaces::log_level::critical;
		default:
			return kcenon::common::interfaces::log_level::info;
	}
}
#endif

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
