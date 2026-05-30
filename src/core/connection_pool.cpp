// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/core/connection_pool.h>

#include <utility>

namespace database
{
namespace core
{
namespace pool
{

// ---------------------------------------------------------------------------
// pooled_connection
// ---------------------------------------------------------------------------

pooled_connection::pooled_connection(std::shared_ptr<connection_pool> pool,
                                     std::unique_ptr<database_backend> backend) noexcept
	: pool_(std::move(pool))
	, backend_(std::move(backend))
{
}

pooled_connection::pooled_connection(pooled_connection&& other) noexcept
	: pool_(std::move(other.pool_))
	, backend_(std::move(other.backend_))
	, broken_(other.broken_)
{
	other.broken_ = false;
}

pooled_connection& pooled_connection::operator=(pooled_connection&& other) noexcept
{
	if (this != &other) {
		release();
		pool_ = std::move(other.pool_);
		backend_ = std::move(other.backend_);
		broken_ = other.broken_;
		other.broken_ = false;
	}
	return *this;
}

pooled_connection::~pooled_connection()
{
	release();
}

void pooled_connection::release() noexcept
{
	if (pool_ && backend_) {
		// Return to pool (noexcept contract: the pool's return_connection
		// must not throw; it only moves pointers and notifies the CV).
		pool_->return_connection(std::move(backend_), broken_);
	}
	pool_.reset();
	backend_.reset();
	broken_ = false;
}

// ---------------------------------------------------------------------------
// connection_pool
// ---------------------------------------------------------------------------

namespace {

connection_validator default_validator()
{
	return [](database_backend& backend) {
		return backend.is_initialized();
	};
}

error_info make_pool_error(error_code code, std::string message)
{
	return error_info{static_cast<int>(code), std::move(message), "connection_pool"};
}

} // namespace

std::shared_ptr<connection_pool> connection_pool::create(
	pool_config config,
	connection_factory factory,
	connection_validator validator)
{
	if (config.max_size == 0) {
		config.max_size = 1;
	}
	if (config.min_size > config.max_size) {
		config.min_size = config.max_size;
	}

	if (!validator) {
		validator = default_validator();
	}

	// Use new because the ctor is private; std::make_shared would need a friend.
	auto pool = std::shared_ptr<connection_pool>(new connection_pool(
		std::move(config), std::move(factory), std::move(validator)));

	// Pre-warm up to min_size. Failures are tolerated: the pool will retry
	// on the first acquire() call.
	{
		std::unique_lock<std::mutex> lock(pool->mutex_);
		while (pool->total_connections_ < pool->config_.min_size) {
			auto backend = pool->create_locked(lock);
			if (!backend) {
				break; // Abort pre-warming; acquire() will retry later.
			}
			pool->idle_.push_back(std::move(backend));
		}
	}

	return pool;
}

connection_pool::connection_pool(pool_config config,
                                 connection_factory factory,
                                 connection_validator validator)
	: config_(std::move(config))
	, factory_(std::move(factory))
	, validator_(std::move(validator))
{
}

connection_pool::~connection_pool()
{
	shutdown();
}

void connection_pool::shutdown()
{
	std::deque<std::unique_ptr<database_backend>> drained;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (shut_down_) {
			return;
		}
		shut_down_ = true;
		drained.swap(idle_);
		total_connections_ -= drained.size();
	}
	// Destroy idle connections outside the lock. shutdown() on each backend
	// is invoked via the backend destructor.
	drained.clear();
	cv_.notify_all();
}

bool connection_pool::is_shut_down() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return shut_down_;
}

pool_statistics connection_pool::stats() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	pool_statistics s;
	s.total_connections = total_connections_;
	s.idle_connections = idle_.size();
	s.active_connections = active_connections_;
	s.waiters = waiters_;
	s.total_acquires = total_acquires_;
	s.acquire_timeouts = acquire_timeouts_;
	s.failed_creations = failed_creations_;
	s.replaced_connections = replaced_connections_;
	return s;
}

Result<pooled_connection> connection_pool::acquire()
{
	return acquire(config_.acquire_timeout);
}

Result<pooled_connection> connection_pool::acquire(std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	std::unique_lock<std::mutex> lock(mutex_);

	for (;;) {
		if (shut_down_) {
			return make_pool_error(error_code::invalid_state,
			                       "connection_pool has been shut down");
		}

		// Prefer an idle connection.
		if (auto backend = take_idle_locked()) {
			// Validate (with the lock released) before handing out.
			if (config_.validate_on_acquire) {
				lock.unlock();
				const bool ok = validator_(*backend);
				lock.lock();
				if (!ok) {
					// Discard and try to create a replacement.
					--total_connections_;
					++replaced_connections_;
					backend.reset();
					auto fresh = create_locked(lock);
					if (!fresh) {
						// Creation failed; loop to wait again or timeout.
					} else {
						++active_connections_;
						++total_acquires_;
						return pooled_connection(shared_from_this(), std::move(fresh));
					}
				} else {
					++active_connections_;
					++total_acquires_;
					return pooled_connection(shared_from_this(), std::move(backend));
				}
			} else {
				++active_connections_;
				++total_acquires_;
				return pooled_connection(shared_from_this(), std::move(backend));
			}
		}

		// No idle connection. Can we create a new one?
		if (total_connections_ < config_.max_size) {
			auto fresh = create_locked(lock);
			if (fresh) {
				++active_connections_;
				++total_acquires_;
				return pooled_connection(shared_from_this(), std::move(fresh));
			}
			// Creation failed; fall through to waiting (so we back off a bit).
		}

		// Pool is saturated (or creation failed): wait for a release.
		++waiters_;
		const auto wait_result = cv_.wait_until(lock, deadline);
		--waiters_;

		if (wait_result == std::cv_status::timeout &&
		    std::chrono::steady_clock::now() >= deadline) {
			++acquire_timeouts_;
			return make_pool_error(error_code::timeout,
			                       "timed out waiting for a pooled connection");
		}
		// Otherwise loop and try again.
	}
}

void connection_pool::return_connection(std::unique_ptr<database_backend> backend, bool broken)
{
	if (!backend) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);

		// Invariant: only decrement active when we were counted as active.
		if (active_connections_ > 0) {
			--active_connections_;
		}

		if (broken || shut_down_) {
			// Drop the backend entirely.
			if (total_connections_ > 0) {
				--total_connections_;
			}
			if (broken) {
				++replaced_connections_;
			}
		} else {
			// Return to idle (LIFO keeps the most-recently-used connection hot).
			idle_.push_back(std::move(backend));
		}
	}

	cv_.notify_one();
}

std::unique_ptr<database_backend> connection_pool::create_locked(std::unique_lock<std::mutex>& lock)
{
	// Factory may block on network I/O; drop the lock while calling it.
	lock.unlock();
	std::unique_ptr<database_backend> backend;
	try {
		backend = factory_();
	} catch (...) {
		backend.reset();
	}
	lock.lock();

	if (!backend) {
		++failed_creations_;
		return nullptr;
	}

	++total_connections_;
	return backend;
}

std::unique_ptr<database_backend> connection_pool::take_idle_locked()
{
	if (idle_.empty()) {
		return nullptr;
	}
	// LIFO: reuse the most recently released connection first.
	auto backend = std::move(idle_.back());
	idle_.pop_back();
	return backend;
}

} // namespace pool
} // namespace core
} // namespace kcenon::database
