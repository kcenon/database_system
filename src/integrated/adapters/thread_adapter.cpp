// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/thread_adapter.h>
#include <kcenon/database/integrated/adapters/backends/thread_backend.h>
#include <kcenon/database/integrated/adapters/backends/null_thread_backend.h>
#include <kcenon/database/integrated/adapters/backends/fallback_thread_backend.h>

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Backend Factory
// ═══════════════════════════════════════════════════════════════

std::unique_ptr<backends::thread_backend> thread_adapter::create_backend(
	const db_thread_config& config,
	thread_backend_type backend_type)
{
	switch (backend_type)
	{
		case thread_backend_type::auto_select:
		case thread_backend_type::fallback:
			return std::make_unique<backends::fallback_thread_backend>(config);

		case thread_backend_type::null:
			return std::make_unique<backends::null_thread_backend>(config);

		default:
			return std::make_unique<backends::fallback_thread_backend>(config);
	}
}

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

thread_adapter::thread_adapter(
	const db_thread_config& config,
	thread_backend_type backend_type)
	: config_(config)
	, backend_(create_backend(config, backend_type))
{
}

thread_adapter::~thread_adapter()
{
	if (backend_ && backend_->is_initialized())
	{
		backend_->shutdown();
	}
}

thread_adapter::thread_adapter(thread_adapter&&) noexcept = default;

common::VoidResult thread_adapter::initialize()
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not created", "" });
	}

	return backend_->initialize();
}

common::VoidResult thread_adapter::shutdown()
{
	if (!backend_)
	{
		return common::ok();
	}

	return backend_->shutdown();
}

bool thread_adapter::is_initialized() const
{
	return backend_ && backend_->is_initialized();
}

// ═══════════════════════════════════════════════════════════════
// Task Execution
// ═══════════════════════════════════════════════════════════════

common::VoidResult thread_adapter::execute(std::function<void()> task)
{
	if (!backend_)
	{
		return common::VoidResult(
			common::error_info{ -1, "Backend not initialized", "" });
	}

	return backend_->execute(std::move(task));
}

// ═══════════════════════════════════════════════════════════════
// Work Completion
// ═══════════════════════════════════════════════════════════════

void thread_adapter::wait_for_completion()
{
	if (backend_)
	{
		backend_->wait_for_completion();
	}
}

bool thread_adapter::wait_for_completion_timeout(std::chrono::milliseconds timeout)
{
	if (!backend_)
	{
		return true;
	}

	return backend_->wait_for_completion_timeout(timeout);
}

// ═══════════════════════════════════════════════════════════════
// Pool Statistics
// ═══════════════════════════════════════════════════════════════

std::size_t thread_adapter::worker_count() const
{
	if (!backend_)
	{
		return 0;
	}

	return backend_->worker_count();
}

std::size_t thread_adapter::queue_size() const
{
	if (!backend_)
	{
		return 0;
	}

	return backend_->queue_size();
}

bool thread_adapter::is_idle() const
{
	if (!backend_)
	{
		return true;
	}

	return backend_->is_idle();
}

} // namespace adapters
} // namespace integrated
} // namespace database
