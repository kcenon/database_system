// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_pool_test.cpp
 * @brief Unit tests for database::core::pool::connection_pool
 *
 * Uses a lightweight in-memory fake backend so the tests exercise the pool's
 * synchronization, bookkeeping, and RAII guarantees without requiring any
 * real database driver.
 */

#include <gtest/gtest.h>

#include "database/core/connection_pool.h"
#include "database/core/database_backend.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using database::core::database_backend;
using database::core::pool::connection_pool;
using database::core::pool::pool_config;
using database::core::pool::pooled_connection;

// A minimal in-memory backend suitable for unit-testing the pool. It honors
// the database_backend contract just enough to be initialized, validated, and
// shut down. Queries are no-ops.
class fake_backend : public database_backend
{
public:
	explicit fake_backend(int id) : id_(id) {}

	int id() const { return id_; }
	void set_valid(bool v) { valid_.store(v); }

	database::database_types type() const override { return database::database_types::none; }

	kcenon::common::VoidResult initialize(const database::core::connection_config&) override
	{
		initialized_.store(true);
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult shutdown() override
	{
		initialized_.store(false);
		return kcenon::common::ok();
	}

	bool is_initialized() const override
	{
		return initialized_.load() && valid_.load();
	}

	kcenon::common::Result<database::core::database_result>
	select_query(const std::string&) override
	{
		return database::core::database_result{};
	}

	kcenon::common::VoidResult execute_query(const std::string&) override
	{
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult begin_transaction() override { return kcenon::common::ok(); }
	kcenon::common::VoidResult commit_transaction() override { return kcenon::common::ok(); }
	kcenon::common::VoidResult rollback_transaction() override { return kcenon::common::ok(); }
	bool in_transaction() const override { return false; }
	std::string last_error() const override { return {}; }
	std::map<std::string, std::string> connection_info() const override { return {}; }

private:
	int id_;
	std::atomic<bool> initialized_{true};
	std::atomic<bool> valid_{true};
};

struct counting_factory
{
	std::atomic<int> creations{0};
	std::atomic<int> fail_next{0};

	std::unique_ptr<database_backend> operator()()
	{
		if (fail_next.load() > 0) {
			fail_next.fetch_sub(1);
			return {};
		}
		int id = creations.fetch_add(1) + 1;
		return std::make_unique<fake_backend>(id);
	}
};

// ---------------------------------------------------------------------------
// Basic lifecycle
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, PrewarmsToMinSize)
{
	pool_config cfg;
	cfg.min_size = 3;
	cfg.max_size = 5;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	auto stats = pool->stats();
	EXPECT_EQ(stats.total_connections, 3u);
	EXPECT_EQ(stats.idle_connections, 3u);
	EXPECT_EQ(stats.active_connections, 0u);
	EXPECT_EQ(factory->creations.load(), 3);
}

TEST(ConnectionPoolTest, AcquireAndReleaseSingleConnection)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 2;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	{
		auto lease = pool->acquire();
		ASSERT_TRUE(lease.is_ok());
		ASSERT_TRUE(lease.value().valid());
		EXPECT_NE(lease.value().get(), nullptr);

		auto stats = pool->stats();
		EXPECT_EQ(stats.active_connections, 1u);
		EXPECT_EQ(stats.idle_connections, 0u);
		EXPECT_EQ(stats.total_connections, 1u);
	}

	auto stats = pool->stats();
	EXPECT_EQ(stats.active_connections, 0u);
	EXPECT_EQ(stats.idle_connections, 1u);
	EXPECT_EQ(stats.total_connections, 1u);
}

TEST(ConnectionPoolTest, LeasesReuseTheSameConnectionLifo)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 4;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	fake_backend* first_ptr = nullptr;
	{
		auto lease = pool->acquire();
		ASSERT_TRUE(lease.is_ok());
		first_ptr = static_cast<fake_backend*>(lease.value().get());
	}
	{
		auto lease = pool->acquire();
		ASSERT_TRUE(lease.is_ok());
		// LIFO reuse: same backend instance as last lease.
		EXPECT_EQ(static_cast<fake_backend*>(lease.value().get()), first_ptr);
	}
	EXPECT_EQ(factory->creations.load(), 1);
}

// ---------------------------------------------------------------------------
// Concurrent checkout/checkin
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, ConcurrentAcquireReleaseIsRaceFree)
{
	pool_config cfg;
	cfg.min_size = 2;
	cfg.max_size = 4;
	cfg.acquire_timeout = 2000ms;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	constexpr int kThreads = 16;
	constexpr int kIterations = 200;

	std::atomic<int> ok_count{0};
	std::atomic<int> fail_count{0};

	std::vector<std::thread> threads;
	threads.reserve(kThreads);
	for (int i = 0; i < kThreads; ++i) {
		threads.emplace_back([&]() {
			for (int j = 0; j < kIterations; ++j) {
				auto lease = pool->acquire();
				if (lease.is_ok()) {
					ASSERT_TRUE(lease.value().valid());
					// Simulate brief use.
					std::this_thread::sleep_for(50us);
					++ok_count;
				} else {
					++fail_count;
				}
			}
		});
	}

	for (auto& t : threads) t.join();

	EXPECT_EQ(ok_count.load(), kThreads * kIterations);
	EXPECT_EQ(fail_count.load(), 0);

	auto stats = pool->stats();
	EXPECT_EQ(stats.active_connections, 0u);
	// Never exceeded max_size.
	EXPECT_LE(stats.total_connections, cfg.max_size);
	EXPECT_LE(static_cast<std::size_t>(factory->creations.load()), cfg.max_size);
}

// ---------------------------------------------------------------------------
// Exhaustion / timeout behavior
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, AcquireTimesOutWhenPoolExhausted)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 1;
	cfg.acquire_timeout = 50ms;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	auto hold = pool->acquire();
	ASSERT_TRUE(hold.is_ok());

	const auto start = std::chrono::steady_clock::now();
	auto second = pool->acquire();
	const auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_TRUE(second.is_err());
	EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
	          45);

	auto stats = pool->stats();
	EXPECT_EQ(stats.acquire_timeouts, 1u);
}

TEST(ConnectionPoolTest, BlockedAcquireUnblocksWhenLeaseReleased)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 1;
	cfg.acquire_timeout = 1s;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	// Hold a lease in a scope we can explicitly end.
	std::optional<pooled_connection> first_holder;
	{
		auto first = pool->acquire();
		ASSERT_TRUE(first.is_ok());
		first_holder.emplace(std::move(first.value()));
	}

	std::atomic<bool> second_got{false};
	std::thread waiter([&]() {
		auto lease = pool->acquire();
		if (lease.is_ok()) second_got.store(true);
	});

	std::this_thread::sleep_for(50ms);
	EXPECT_FALSE(second_got.load());

	// Release the first lease; the waiter should unblock.
	first_holder.reset();

	waiter.join();
	EXPECT_TRUE(second_got.load());
}

// ---------------------------------------------------------------------------
// Broken connection replacement
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, MarkBrokenDiscardsConnection)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 2;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	{
		auto lease = pool->acquire();
		ASSERT_TRUE(lease.is_ok());
		lease.value().mark_broken();
	}

	auto stats = pool->stats();
	EXPECT_EQ(stats.active_connections, 0u);
	EXPECT_EQ(stats.idle_connections, 0u);
	EXPECT_EQ(stats.total_connections, 0u);
	EXPECT_EQ(stats.replaced_connections, 1u);

	// Next acquire must create a new connection.
	auto lease = pool->acquire();
	ASSERT_TRUE(lease.is_ok());
	EXPECT_EQ(factory->creations.load(), 2);
}

TEST(ConnectionPoolTest, InvalidIdleConnectionIsReplacedOnAcquire)
{
	pool_config cfg;
	cfg.min_size = 1;
	cfg.max_size = 2;
	cfg.validate_on_acquire = true;

	auto factory = std::make_shared<counting_factory>();
	// Keep a raw pointer to the first fake created via the factory so the
	// test can flip its validity.
	std::shared_ptr<fake_backend*> first_raw = std::make_shared<fake_backend*>(nullptr);
	auto creator = [factory, first_raw]() -> std::unique_ptr<database_backend> {
		auto backend = (*factory)();
		if (backend && *first_raw == nullptr) {
			*first_raw = static_cast<fake_backend*>(backend.get());
		}
		return backend;
	};
	auto pool = connection_pool::create(cfg, creator);

	ASSERT_NE(*first_raw, nullptr);
	// Simulate the idle connection going bad (e.g., server-side timeout).
	(*first_raw)->set_valid(false);

	auto lease = pool->acquire();
	ASSERT_TRUE(lease.is_ok());
	// Replacement has been created.
	EXPECT_GE(factory->creations.load(), 2);

	auto stats = pool->stats();
	EXPECT_GE(stats.replaced_connections, 1u);
}

// ---------------------------------------------------------------------------
// Factory failure handling
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, FactoryFailureIsSurfacedWhenPoolEmpty)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 1;
	cfg.acquire_timeout = 50ms;

	auto factory = std::make_shared<counting_factory>();
	factory->fail_next.store(10); // All factory calls fail.

	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	auto lease = pool->acquire();
	EXPECT_TRUE(lease.is_err());

	auto stats = pool->stats();
	EXPECT_GE(stats.failed_creations, 1u);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, AcquireAfterShutdownFails)
{
	pool_config cfg;
	cfg.min_size = 1;
	cfg.max_size = 2;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	pool->shutdown();

	auto lease = pool->acquire();
	EXPECT_TRUE(lease.is_err());
}

TEST(ConnectionPoolTest, ShutdownUnblocksWaitingAcquire)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 1;
	cfg.acquire_timeout = 5s;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	// Hold one lease so the next acquire has to block.
	std::optional<pooled_connection> holder;
	{
		auto first = pool->acquire();
		ASSERT_TRUE(first.is_ok());
		holder.emplace(std::move(first.value()));
	}

	std::atomic<bool> returned{false};
	std::thread waiter([&]() {
		auto lease = pool->acquire();
		(void)lease;
		returned.store(true);
	});

	std::this_thread::sleep_for(50ms);
	EXPECT_FALSE(returned.load());

	pool->shutdown();

	waiter.join();
	EXPECT_TRUE(returned.load());
	holder.reset();
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST(ConnectionPoolTest, PooledConnectionMoveTransfersLease)
{
	pool_config cfg;
	cfg.min_size = 0;
	cfg.max_size = 1;

	auto factory = std::make_shared<counting_factory>();
	auto pool = connection_pool::create(cfg, [factory]() { return (*factory)(); });

	auto result = pool->acquire();
	ASSERT_TRUE(result.is_ok());

	pooled_connection original = std::move(result.value());
	ASSERT_TRUE(original.valid());

	pooled_connection moved = std::move(original);
	EXPECT_TRUE(moved.valid());
	EXPECT_FALSE(original.valid());

	auto stats = pool->stats();
	EXPECT_EQ(stats.active_connections, 1u);
}

} // namespace
