// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "logger_adapter.h"
#include "backends/logger_backend.h"
#include "backends/null_logger_backend.h"
#include "backends/fallback_logger_backend.h"

// Include system backend only if built
// HAVE_SYSTEM_LOGGER_BACKEND is defined by CMake when system_logger_backend.cpp is compiled
#ifdef HAVE_SYSTEM_LOGGER_BACKEND
	#include "backends/system_logger_backend.h"
#endif

#include <algorithm>
#include <sstream>

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════

namespace
{

/**
 * @brief Sanitize SQL query for logging
 *
 * Removes sensitive information and truncates long queries.
 */
std::string sanitize_query(const std::string& query)
{
	std::string sanitized = query;

	// Remove password patterns
	const std::string patterns[]
		= { "PASSWORD '", "PASSWORD=", "password '", "password=", "PWD=", "pwd=" };

	for (const auto& pattern : patterns)
	{
		size_t pos = sanitized.find(pattern);
		if (pos != std::string::npos)
		{
			size_t end = sanitized.find_first_of("' \t\n;", pos + pattern.length());
			if (end != std::string::npos)
			{
				sanitized.replace(pos + pattern.length(), end - (pos + pattern.length()), "***");
			}
		}
	}

	// Truncate if too long
	const size_t max_length = 500;
	if (sanitized.length() > max_length)
	{
		sanitized = sanitized.substr(0, max_length) + "... [truncated]";
	}

	return sanitized;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// Backend Factory
// ═══════════════════════════════════════════════════════════════

std::unique_ptr<backends::logger_backend> logger_adapter::create_backend(
	const db_logger_config& config,
	logger_backend_type backend_type)
{
	switch (backend_type)
	{
		case logger_backend_type::auto_select:
		{
#ifdef HAVE_SYSTEM_LOGGER_BACKEND
			// Try system backend first
			try
			{
				auto backend = std::make_unique<backends::system_logger_backend>(config);
				auto init_result = backend->initialize();
				if (init_result.is_ok())
				{
					return backend;
				}
				// If initialization failed, fall back to fallback backend
			}
			catch (...)
			{
				// If system backend construction/init failed, fall back
			}
#endif
			// Fall back to fallback_logger_backend
			return std::make_unique<backends::fallback_logger_backend>(config);
		}

		case logger_backend_type::system:
		{
#ifdef HAVE_SYSTEM_LOGGER_BACKEND
			return std::make_unique<backends::system_logger_backend>(config);
#else
			throw std::runtime_error(
				"system_logger_backend not available (logger_system not found)");
#endif
		}

		case logger_backend_type::fallback:
			return std::make_unique<backends::fallback_logger_backend>(config);

		case logger_backend_type::null:
			return std::make_unique<backends::null_logger_backend>(config);

		default:
			return std::make_unique<backends::fallback_logger_backend>(config);
	}
}

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

logger_adapter::logger_adapter(
	const db_logger_config& config,
	logger_backend_type backend_type)
	: config_(config)
	, backend_(create_backend(config, backend_type))
{
	// Backend is created and potentially initialized (if auto_select)
	// If auto_select didn't initialize, caller must call initialize()
	if (backend_type != logger_backend_type::auto_select)
	{
		// For explicit backend selection, don't auto-initialize
		// Caller must call initialize() explicitly
	}
}

logger_adapter::~logger_adapter()
{
	if (backend_ && backend_->is_initialized())
	{
		backend_->shutdown();
	}
}

logger_adapter::logger_adapter(logger_adapter&&) noexcept = default;

common::VoidResult logger_adapter::initialize()
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not created", "" });
	}

	return backend_->initialize();
}

common::VoidResult logger_adapter::shutdown()
{
	if (!backend_)
	{
		return common::ok();
	}

	return backend_->shutdown();
}

bool logger_adapter::is_initialized() const
{
	return backend_ && backend_->is_initialized();
}

// ═══════════════════════════════════════════════════════════════
// Database-Specific Logging
// ═══════════════════════════════════════════════════════════════

void logger_adapter::log_query(
	db_log_level level, const std::string& query, std::chrono::microseconds duration)
{
	if (!backend_)
	{
		return;
	}

	// Sanitize query
	std::string safe_query = sanitize_query(query);

	// Format message
	std::stringstream ss;
	ss << "Query executed in " << duration.count() << "μs: " << safe_query;

	backend_->log(level, ss.str());

	// Check for slow query
	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	if (config_.log_slow_queries && duration_ms > config_.slow_query_threshold)
	{
		log_slow_query(query, duration, config_.slow_query_threshold);
	}
}

void logger_adapter::log_slow_query(const std::string& query, std::chrono::microseconds duration,
	std::chrono::milliseconds threshold)
{
	if (!backend_)
	{
		return;
	}

	std::string safe_query = sanitize_query(query);

	std::stringstream ss;
	ss << "SLOW QUERY detected (threshold: " << threshold.count() << "ms, actual: "
	   << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
	   << "ms): " << safe_query;

	backend_->log(db_log_level::warning, ss.str());
}

void logger_adapter::log_connection_event(const std::string& event, const std::string& details)
{
	if (!backend_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Connection event [" << event << "]: " << details;

	backend_->log(db_log_level::debug, ss.str());
}

void logger_adapter::log_transaction(
	const std::string& operation, bool success, const std::string& details)
{
	if (!backend_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Transaction " << operation << " " << (success ? "SUCCESS" : "FAILED");
	if (!details.empty())
	{
		ss << ": " << details;
	}

	backend_->log(success ? db_log_level::info : db_log_level::error, ss.str());
}

void logger_adapter::log_pool_event(const std::string& event, std::size_t active, std::size_t idle)
{
	if (!backend_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Pool event [" << event << "]: active=" << active << ", idle=" << idle
	   << ", total=" << (active + idle);

	backend_->log(db_log_level::info, ss.str());
}

void logger_adapter::log_error(
	const std::string& operation, const std::string& error_msg, const std::string& sql_state)
{
	if (!backend_)
	{
		return;
	}

	std::stringstream ss;
	ss << "ERROR in " << operation << ": " << error_msg;
	if (!sql_state.empty())
	{
		ss << " (SQL state: " << sql_state << ")";
	}

	backend_->log(db_log_level::error, ss.str());
}

void logger_adapter::log(db_log_level level, const std::string& message)
{
	if (backend_)
	{
		backend_->log(level, message);
	}
}

void logger_adapter::flush()
{
	if (backend_)
	{
		backend_->flush();
	}
}

} // namespace adapters
} // namespace integrated
} // namespace database
