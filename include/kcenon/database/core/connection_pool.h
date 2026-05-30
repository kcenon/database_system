// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_pool.h
 * @brief Thread-safe connection pool for database backends
 *
 * Provides a bounded, thread-safe pool of database_backend instances with
 * RAII-based lease handles (pooled_connection). The pool is backend-agnostic:
 * any object implementing the database::core::database_backend interface can
 * be pooled, regardless of the underlying driver (libpqxx, sqlite3, etc.).
 *
 * Design goals:
 * - Thread-safe acquisition and release (mutex + condition variable)
 * - Bounded size with min/max connection counts
 * - RAII lease handle: pooled_connection automatically returns on scope exit
 * - Connection validation on checkout: broken connections are replaced
 * - Timeout-based backpressure: acquire() returns error on exhaustion
 * - No dependency on a specific backend implementation
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - pooled_connection is move-only; the wrapped backend is accessed by one
 *   thread at a time (the lease holder)
 *
 * Example:
 * @code
 * using namespace database::core;
 *
 * pool::pool_config cfg;
 * cfg.min_size = 2;
 * cfg.max_size = 8;
 * cfg.acquire_timeout = std::chrono::seconds(5);
 *
 * auto pool = std::make_shared<pool::connection_pool>(
 *     cfg,
 *     [config]() {
 *         auto backend = backend_registry::instance().create("postgresql");
 *         auto init_result = backend->initialize(config);
 *         if (init_result.is_err()) return std::unique_ptr<database_backend>{};
 *         return backend;
 *     });
 *
 * {
 *     auto lease = pool->acquire();
 *     if (lease.is_ok()) {
 *         auto rows = lease.value()->select_query("SELECT 1");
 *     }
 * } // Connection returned to pool here
 * @endcode
 */

#pragma once

#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/core/result.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace database
{
namespace core
{
namespace pool
{

/**
 * @struct pool_config
 * @brief Configuration for a connection_pool instance.
 *
 * Reasonable defaults are provided for moderate workloads. Tune `max_size`
 * for high-concurrency scenarios and `acquire_timeout` to balance back-pressure
 * against caller latency.
 */
struct pool_config
{
	/// Minimum number of connections. Pool pre-creates this many on startup.
	std::size_t min_size = 1;

	/// Maximum number of connections the pool will hold simultaneously.
	std::size_t max_size = 8;

	/// How long acquire() waits for an idle connection before failing.
	std::chrono::milliseconds acquire_timeout = std::chrono::milliseconds(5000);

	/// Whether to validate a connection before returning it from acquire().
	/// If true, the pool invokes the validator callback; on failure the
	/// connection is discarded and a new one is created.
	bool validate_on_acquire = true;
};

/**
 * @brief Factory callable that produces a new, initialized database_backend.
 *
 * Called by the pool when it needs to create a new connection (lazily up to
 * max_size, or to replace a failed one). The backend must be returned in the
 * initialized state. If creation fails, return an empty unique_ptr; the pool
 * will surface the error to the caller of acquire().
 */
using connection_factory = std::function<std::unique_ptr<database_backend>()>;

/**
 * @brief Validator callable: returns true if the backend is still usable.
 *
 * The default validator checks `is_initialized()`. Backend-specific validators
 * may issue a lightweight "SELECT 1"-style query instead.
 */
using connection_validator = std::function<bool(database_backend&)>;

class connection_pool;

/**
 * @class pooled_connection
 * @brief Move-only RAII lease handle for a pooled database_backend.
 *
 * While this handle is alive, the caller has exclusive access to the wrapped
 * backend. When the handle is destroyed (or reset), the backend is returned
 * to the pool and becomes available to other callers. Access the backend via
 * `operator->` or `get()`.
 *
 * Invariant: a valid pooled_connection always wraps a non-null backend and
 * references a live pool.
 */
class pooled_connection
{
public:
	/// Construct an empty (invalid) handle.
	pooled_connection() = default;

	/// Construct a lease referencing a backend checked out from `pool`.
	pooled_connection(std::shared_ptr<connection_pool> pool,
	                  std::unique_ptr<database_backend> backend) noexcept;

	/// Move constructor transfers the lease.
	pooled_connection(pooled_connection&& other) noexcept;

	/// Move assignment releases any current lease, then transfers from `other`.
	pooled_connection& operator=(pooled_connection&& other) noexcept;

	pooled_connection(const pooled_connection&) = delete;
	pooled_connection& operator=(const pooled_connection&) = delete;

	/// Destructor returns the backend to the pool (if any).
	~pooled_connection();

	/// Access the underlying backend. Precondition: valid() is true.
	database_backend* operator->() const noexcept { return backend_.get(); }

	/// Access the underlying backend. Precondition: valid() is true.
	database_backend& operator*() const noexcept { return *backend_; }

	/// Raw backend pointer (may be nullptr if the lease is empty/moved-from).
	database_backend* get() const noexcept { return backend_.get(); }

	/// True if the handle holds a live backend.
	[[nodiscard]] bool valid() const noexcept { return backend_ != nullptr; }

	explicit operator bool() const noexcept { return valid(); }

	/**
	 * @brief Mark the wrapped connection as broken.
	 *
	 * On release, a broken connection is discarded instead of returned to the
	 * idle queue. Call this when a query fails in a way that indicates the
	 * underlying connection is no longer usable (e.g., protocol error).
	 */
	void mark_broken() noexcept { broken_ = true; }

	/**
	 * @brief Release the lease early, returning the backend to the pool.
	 *
	 * Equivalent to destroying the handle; the handle becomes invalid.
	 */
	void release() noexcept;

private:
	std::shared_ptr<connection_pool> pool_;
	std::unique_ptr<database_backend> backend_;
	bool broken_ = false;
};

/**
 * @struct pool_statistics
 * @brief Snapshot of pool state for monitoring.
 *
 * Returned by connection_pool::stats(). Values are a consistent snapshot
 * captured under the pool mutex.
 */
struct pool_statistics
{
	std::size_t total_connections = 0;   ///< Currently living connections (idle + active).
	std::size_t idle_connections = 0;    ///< Connections in the idle queue.
	std::size_t active_connections = 0;  ///< Leased connections (currently in use).
	std::size_t waiters = 0;             ///< Threads currently blocked in acquire().

	std::uint64_t total_acquires = 0;    ///< Successful acquire calls since creation.
	std::uint64_t acquire_timeouts = 0;  ///< Acquires that failed due to timeout.
	std::uint64_t failed_creations = 0;  ///< Factory invocations that returned null.
	std::uint64_t replaced_connections = 0; ///< Broken/invalid connections replaced.
};

/**
 * @class connection_pool
 * @brief Thread-safe pool of database_backend instances.
 *
 * Create the pool with a factory that produces initialized backend instances.
 * Call `acquire()` to obtain a lease (returns pooled_connection); the lease is
 * automatically returned when destroyed.
 *
 * The pool lazily creates connections up to `max_size`. Idle connections are
 * kept in a LIFO queue for cache-warmth. On `shutdown()` or destruction, all
 * connections are drained and disposed.
 *
 * Must be held via std::shared_ptr (pooled_connection holds a weak reference
 * back to the pool via shared_ptr). Use the static `create()` helper.
 */
class connection_pool : public std::enable_shared_from_this<connection_pool>
{
public:
	/**
	 * @brief Create a new connection_pool.
	 * @param config Pool sizing and timeout configuration.
	 * @param factory Callable that produces new initialized backends.
	 * @param validator Optional validator (default: checks is_initialized()).
	 * @return Shared pointer to the pool. Pre-warms to min_size on success.
	 *
	 * If pre-warming fails (factory returns null for the very first call),
	 * the pool is still returned but will retry creation on acquire().
	 */
	static std::shared_ptr<connection_pool> create(
		pool_config config,
		connection_factory factory,
		connection_validator validator = {});

	~connection_pool();

	connection_pool(const connection_pool&) = delete;
	connection_pool& operator=(const connection_pool&) = delete;
	connection_pool(connection_pool&&) = delete;
	connection_pool& operator=(connection_pool&&) = delete;

	/**
	 * @brief Acquire an idle backend from the pool.
	 *
	 * Blocks up to `config.acquire_timeout` waiting for a connection. If a
	 * connection can be created (total < max_size), one is created on demand.
	 * If validation is enabled, the returned connection is validated before
	 * hand-off; an invalid one is discarded and a replacement is created.
	 *
	 * @return Result containing a valid pooled_connection, or an error_info
	 *         describing the failure (timeout, creation failure, shutdown).
	 *
	 * Thread-safe.
	 */
	[[nodiscard]] Result<pooled_connection> acquire();

	/**
	 * @brief Acquire with an explicit timeout (overrides the configured one).
	 */
	[[nodiscard]] Result<pooled_connection> acquire(std::chrono::milliseconds timeout);

	/**
	 * @brief Get a consistent snapshot of pool statistics.
	 *
	 * Thread-safe.
	 */
	pool_statistics stats() const;

	/**
	 * @brief Shut down the pool: drain all idle connections and prevent
	 *        further acquires.
	 *
	 * Active leases are not forcibly revoked; they must still be returned
	 * by their holders (at which point the connection is disposed). After
	 * shutdown(), acquire() returns an invalid_state error.
	 *
	 * Safe to call multiple times.
	 */
	void shutdown();

	/**
	 * @brief True if the pool has been shut down.
	 */
	[[nodiscard]] bool is_shut_down() const;

	/// The effective configuration.
	const pool_config& config() const noexcept { return config_; }

private:
	friend class pooled_connection;

	connection_pool(pool_config config,
	                connection_factory factory,
	                connection_validator validator);

	/// Return a backend to the pool (called by pooled_connection on release).
	void return_connection(std::unique_ptr<database_backend> backend, bool broken);

	/// Try to create a new connection; increments total_connections on success.
	/// Called with mutex held. Returns nullptr on factory failure.
	std::unique_ptr<database_backend> create_locked(std::unique_lock<std::mutex>& lock);

	/// Pop an idle connection (if any). Called with mutex held.
	std::unique_ptr<database_backend> take_idle_locked();

	pool_config config_;
	connection_factory factory_;
	connection_validator validator_;

	mutable std::mutex mutex_;
	std::condition_variable cv_;

	std::deque<std::unique_ptr<database_backend>> idle_;
	std::size_t total_connections_ = 0;
	std::size_t active_connections_ = 0;
	std::size_t waiters_ = 0;

	std::uint64_t total_acquires_ = 0;
	std::uint64_t acquire_timeouts_ = 0;
	std::uint64_t failed_creations_ = 0;
	std::uint64_t replaced_connections_ = 0;

	bool shut_down_ = false;
};

} // namespace pool
} // namespace core
} // namespace kcenon::database
