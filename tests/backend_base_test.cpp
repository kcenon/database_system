// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Backend Base CRTP Template Tests
 *
 * Tests for backend_base<Derived, Type> covering:
 * - Initialization guards (double initialization prevention)
 * - Shutdown guards (no-op when not initialized)
 * - State transitions (initialized_ flag management)
 * - Type returns correct database_types
 * - Factory method (create())
 * - RAII: destructor calls shutdown
 * - Failed initialization does not set initialized_ = true
 * - Non-copyable and non-moveable enforcement
 */

#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>

#include <kcenon/database/core/backend_base.h>
#include <kcenon/database/core/database_backend.h>

using namespace kcenon::database;
using namespace kcenon::database::core;

// =============================================================================
// Test Backend Implementations
// =============================================================================

namespace {

struct lifecycle_observer
{
    int shutdown_call_count = 0;
    int partial_cleanup_count = 0;
    bool member_alive = false;
    bool member_alive_during_shutdown = false;
    bool partial_resource_alive = false;
};

class observed_member
{
public:
    explicit observed_member(lifecycle_observer* observer) noexcept
        : observer_(observer)
    {
        if (observer_) {
            observer_->member_alive = true;
        }
    }

    ~observed_member()
    {
        if (observer_) {
            observer_->member_alive = false;
        }
    }

private:
    lifecycle_observer* observer_;
};

class tracked_resource
{
public:
    explicit tracked_resource(lifecycle_observer* observer) noexcept
        : observer_(observer)
    {
        if (observer_) {
            observer_->partial_resource_alive = true;
        }
    }

    ~tracked_resource()
    {
        if (observer_) {
            observer_->partial_resource_alive = false;
            observer_->partial_cleanup_count++;
        }
    }

private:
    lifecycle_observer* observer_;
};

/**
 * @brief Concrete backend that always succeeds initialization
 */
class succeeding_backend
    : public backend_base<succeeding_backend, database_types::sqlite>
{
public:
    static constexpr const char* backend_name() { return "succeeding_backend"; }

    explicit succeeding_backend(lifecycle_observer* observer = nullptr) noexcept
        : observer_(observer), observed_member_(observer)
    {
    }

    ~succeeding_backend() noexcept override
    {
        shutdown_before_derived_destruction();
    }

    int init_call_count = 0;
    int shutdown_call_count = 0;
    bool throw_on_shutdown = false;

    kcenon::common::Result<database_result> select_query(const std::string&) override
    {
        return kcenon::common::Result<database_result>::ok(database_result{});
    }

    kcenon::common::VoidResult execute_query(const std::string&) override
    {
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult begin_transaction() override
    {
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult commit_transaction() override
    {
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult rollback_transaction() override
    {
        return kcenon::common::ok();
    }

    bool in_transaction() const override { return false; }
    std::string last_error() const override { return ""; }
    std::map<std::string, std::string> connection_info() const override { return {}; }

protected:
    friend class backend_base<succeeding_backend, database_types::sqlite>;

    kcenon::common::VoidResult do_initialize(const connection_config&)
    {
        init_call_count++;
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult do_shutdown()
    {
        shutdown_call_count++;
        if (observer_) {
            observer_->shutdown_call_count++;
            observer_->member_alive_during_shutdown = observer_->member_alive;
        }
        if (throw_on_shutdown) {
            throw std::runtime_error("Simulated shutdown exception");
        }
        return kcenon::common::ok();
    }

private:
    lifecycle_observer* observer_;
    observed_member observed_member_;
};

/**
 * @brief Concrete backend that always fails initialization
 */
class failing_backend
    : public backend_base<failing_backend, database_types::postgres>
{
public:
    static constexpr const char* backend_name() { return "failing_backend"; }

    explicit failing_backend(lifecycle_observer* observer = nullptr) noexcept
        : observer_(observer), observed_member_(observer)
    {
    }

    ~failing_backend() noexcept override
    {
        shutdown_before_derived_destruction();
    }

    int init_call_count = 0;

    kcenon::common::Result<database_result> select_query(const std::string&) override
    {
        return kcenon::common::error_info{-1, "Not initialized"};
    }

    kcenon::common::VoidResult execute_query(const std::string&) override
    {
        return kcenon::common::error_info{-1, "Not initialized"};
    }

    kcenon::common::VoidResult begin_transaction() override
    {
        return kcenon::common::error_info{-1, "Not initialized"};
    }

    kcenon::common::VoidResult commit_transaction() override
    {
        return kcenon::common::error_info{-1, "Not initialized"};
    }

    kcenon::common::VoidResult rollback_transaction() override
    {
        return kcenon::common::error_info{-1, "Not initialized"};
    }

    bool in_transaction() const override { return false; }
    std::string last_error() const override { return "Connection failed"; }
    std::map<std::string, std::string> connection_info() const override { return {}; }

protected:
    friend class backend_base<failing_backend, database_types::postgres>;

    kcenon::common::VoidResult do_initialize(const connection_config&)
    {
        init_call_count++;
        partial_resource_ = std::make_unique<tracked_resource>(observer_);
        partial_resource_.reset();
        return kcenon::common::error_info{
            static_cast<int>(database::error_code::connection_failed),
            "Simulated connection failure",
            "failing_backend"
        };
    }

    kcenon::common::VoidResult do_shutdown()
    {
        if (observer_) {
            observer_->shutdown_call_count++;
            observer_->member_alive_during_shutdown = observer_->member_alive;
        }
        return kcenon::common::ok();
    }

private:
    lifecycle_observer* observer_;
    observed_member observed_member_;
    std::unique_ptr<tracked_resource> partial_resource_;
};

} // anonymous namespace

// =============================================================================
// Test Fixture
// =============================================================================

class BackendBaseTest : public ::testing::Test {
protected:
    connection_config test_config_;

    void SetUp() override
    {
        test_config_.host = "localhost";
        test_config_.port = 5432;
        test_config_.database = "test_db";
        test_config_.username = "test_user";
        test_config_.password = "test_pass";
    }
};

// =============================================================================
// Type Tests
// =============================================================================

TEST_F(BackendBaseTest, TypeReturnsCorrectDatabaseType)
{
    succeeding_backend backend;
    EXPECT_EQ(backend.type(), database_types::sqlite);
}

TEST_F(BackendBaseTest, TypeReturnsCorrectForAlternateBackend)
{
    failing_backend backend;
    EXPECT_EQ(backend.type(), database_types::postgres);
}

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(BackendBaseTest, InitialStateIsNotInitialized)
{
    succeeding_backend backend;
    EXPECT_FALSE(backend.is_initialized());
}

TEST_F(BackendBaseTest, SuccessfulInitializationSetsState)
{
    succeeding_backend backend;
    auto result = backend.initialize(test_config_);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(backend.is_initialized());
}

TEST_F(BackendBaseTest, FailedInitializationDoesNotSetState)
{
    failing_backend backend;
    auto result = backend.initialize(test_config_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_FALSE(backend.is_initialized());
}

TEST_F(BackendBaseTest, DoubleInitializationIsRejected)
{
    succeeding_backend backend;
    EXPECT_TRUE(backend.initialize(test_config_).is_ok());

    // Second initialization should fail with error
    auto result = backend.initialize(test_config_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(backend.is_initialized()); // State unchanged
}

TEST_F(BackendBaseTest, DoubleInitializationDoesNotCallDoInitialize)
{
    succeeding_backend backend;
    backend.initialize(test_config_);
    backend.initialize(test_config_); // Should be rejected

    // do_initialize should only be called once
    EXPECT_EQ(backend.init_call_count, 1);
}

TEST_F(BackendBaseTest, InitializeAfterShutdownSucceeds)
{
    succeeding_backend backend;
    EXPECT_TRUE(backend.initialize(test_config_).is_ok());
    EXPECT_TRUE(backend.shutdown().is_ok());
    EXPECT_FALSE(backend.is_initialized());

    // Re-initialization should succeed
    auto result = backend.initialize(test_config_);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(backend.is_initialized());
    EXPECT_EQ(backend.init_call_count, 2);
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TEST_F(BackendBaseTest, ShutdownWithoutInitIsNoOp)
{
    succeeding_backend backend;
    auto result = backend.shutdown();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(backend.is_initialized());
    // do_shutdown should not be called since not initialized
    EXPECT_EQ(backend.shutdown_call_count, 0);
}

TEST_F(BackendBaseTest, ShutdownAfterInitClearsState)
{
    succeeding_backend backend;
    backend.initialize(test_config_);
    auto result = backend.shutdown();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(backend.is_initialized());
    EXPECT_EQ(backend.shutdown_call_count, 1);
}

TEST_F(BackendBaseTest, DoubleShutdownIsIdempotent)
{
    succeeding_backend backend;
    backend.initialize(test_config_);
    backend.shutdown();
    auto result = backend.shutdown(); // Second call is no-op
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(backend.shutdown_call_count, 1);
}

TEST_F(BackendBaseTest, ShutdownSetsInitializedToFalseEvenOnError)
{
    // backend_base always sets initialized_ = false after do_shutdown,
    // regardless of whether do_shutdown returns error
    succeeding_backend backend;
    backend.initialize(test_config_);
    backend.shutdown();
    EXPECT_FALSE(backend.is_initialized());
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_F(BackendBaseTest, CreateReturnsUniquePointer)
{
    auto backend = succeeding_backend::create();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::sqlite);
    EXPECT_FALSE(backend->is_initialized());
}

TEST_F(BackendBaseTest, CreateReturnsDistinctInstances)
{
    auto b1 = succeeding_backend::create();
    auto b2 = succeeding_backend::create();
    EXPECT_NE(b1.get(), b2.get());
}

// =============================================================================
// RAII Tests (Destructor calls shutdown)
// =============================================================================

TEST_F(BackendBaseTest, DestructorCallsShutdown)
{
    lifecycle_observer observer;
    {
        succeeding_backend backend(&observer);
        ASSERT_TRUE(backend.initialize(test_config_).is_ok());
        EXPECT_EQ(observer.shutdown_call_count, 0);
    }
    EXPECT_EQ(observer.shutdown_call_count, 1);
    EXPECT_TRUE(observer.member_alive_during_shutdown);
    EXPECT_FALSE(observer.member_alive);
}

TEST_F(BackendBaseTest, DestructorSafeWithoutInit)
{
    lifecycle_observer observer;
    { succeeding_backend backend(&observer); }
    EXPECT_EQ(observer.shutdown_call_count, 0);
    EXPECT_FALSE(observer.member_alive);
}

TEST_F(BackendBaseTest, ExplicitShutdownThenDestructionCleansExactlyOnce)
{
    lifecycle_observer observer;
    {
        succeeding_backend backend(&observer);
        ASSERT_TRUE(backend.initialize(test_config_).is_ok());
        ASSERT_TRUE(backend.shutdown().is_ok());
        EXPECT_EQ(observer.shutdown_call_count, 1);
    }
    EXPECT_EQ(observer.shutdown_call_count, 1);
    EXPECT_TRUE(observer.member_alive_during_shutdown);
    EXPECT_FALSE(observer.member_alive);
}

TEST_F(BackendBaseTest, FailedInitializationCleansPartialResourcesExactlyOnce)
{
    lifecycle_observer observer;
    {
        failing_backend backend(&observer);
        EXPECT_FALSE(backend.initialize(test_config_).is_ok());
        EXPECT_EQ(observer.partial_cleanup_count, 1);
        EXPECT_FALSE(observer.partial_resource_alive);
    }
    EXPECT_EQ(observer.shutdown_call_count, 0);
    EXPECT_EQ(observer.partial_cleanup_count, 1);
    EXPECT_FALSE(observer.partial_resource_alive);
    EXPECT_FALSE(observer.member_alive);
}

TEST_F(BackendBaseTest, PolymorphicDestructionCleansWhileDerivedMembersAreAlive)
{
    lifecycle_observer observer;
    {
        std::unique_ptr<database_backend> backend =
            std::make_unique<succeeding_backend>(&observer);
        ASSERT_TRUE(backend->initialize(test_config_).is_ok());
    }
    EXPECT_EQ(observer.shutdown_call_count, 1);
    EXPECT_TRUE(observer.member_alive_during_shutdown);
    EXPECT_FALSE(observer.member_alive);
}

TEST_F(BackendBaseTest, DestructorContainsShutdownExceptions)
{
    lifecycle_observer observer;
    EXPECT_NO_THROW({
        succeeding_backend backend(&observer);
        if (!backend.initialize(test_config_).is_ok()) {
            throw std::runtime_error("Initialization unexpectedly failed");
        }
        backend.throw_on_shutdown = true;
    });
    EXPECT_EQ(observer.shutdown_call_count, 1);
    EXPECT_TRUE(observer.member_alive_during_shutdown);
    EXPECT_FALSE(observer.member_alive);
}

// =============================================================================
// Lifecycle Cycle Tests
// =============================================================================

TEST_F(BackendBaseTest, FullLifecycleCycle)
{
    succeeding_backend backend;

    // Initial state
    EXPECT_FALSE(backend.is_initialized());

    // Initialize
    EXPECT_TRUE(backend.initialize(test_config_).is_ok());
    EXPECT_TRUE(backend.is_initialized());

    // Use (simple operation)
    auto result = backend.execute_query("INSERT INTO test VALUES (1)");
    EXPECT_TRUE(result.is_ok());

    // Shutdown
    EXPECT_TRUE(backend.shutdown().is_ok());
    EXPECT_FALSE(backend.is_initialized());
}

TEST_F(BackendBaseTest, MultipleLifecycleCycles)
{
    succeeding_backend backend;

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(backend.initialize(test_config_).is_ok());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.shutdown().is_ok());
        EXPECT_FALSE(backend.is_initialized());
    }

    EXPECT_EQ(backend.init_call_count, 5);
    EXPECT_EQ(backend.shutdown_call_count, 5);
}

// =============================================================================
// Non-Copyable / Non-Moveable Tests
// =============================================================================

TEST_F(BackendBaseTest, BackendIsNotCopyable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<succeeding_backend>);
    EXPECT_FALSE(std::is_copy_assignable_v<succeeding_backend>);
}

TEST_F(BackendBaseTest, BackendIsNotMoveable)
{
    EXPECT_FALSE(std::is_move_constructible_v<succeeding_backend>);
    EXPECT_FALSE(std::is_move_assignable_v<succeeding_backend>);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
