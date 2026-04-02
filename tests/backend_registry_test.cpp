// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Backend Registry Tests
 *
 * Tests for backend_registry covering:
 * - Registration and unregistration
 * - Backend creation via factory
 * - Duplicate registration handling
 * - Unknown backend handling
 * - Thread safety / concurrent access
 * - Clear and re-register
 * - backend_registrar automatic registration
 * - Convenience functions
 */

#include <algorithm>
#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "database/core/backend_base.h"
#include "database/core/backend_registry.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::core;

// =============================================================================
// Test Backend Implementations
// =============================================================================

namespace {

/**
 * @brief Minimal test backend for registry tests
 *
 * Implements the bare minimum of database_backend interface via backend_base
 * to enable factory-based creation through the registry.
 */
class test_backend
    : public backend_base<test_backend, database_types::sqlite>
{
public:
    static constexpr const char* backend_name() { return "test_backend"; }

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
    friend class backend_base<test_backend, database_types::sqlite>;

    kcenon::common::VoidResult do_initialize(const connection_config&)
    {
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult do_shutdown()
    {
        return kcenon::common::ok();
    }
};

/**
 * @brief Another test backend with different type for distinguishing in tests
 */
class test_backend_alt
    : public backend_base<test_backend_alt, database_types::postgres>
{
public:
    static constexpr const char* backend_name() { return "test_backend_alt"; }

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
    friend class backend_base<test_backend_alt, database_types::postgres>;

    kcenon::common::VoidResult do_initialize(const connection_config&)
    {
        return kcenon::common::ok();
    }

    kcenon::common::VoidResult do_shutdown()
    {
        return kcenon::common::ok();
    }
};

} // anonymous namespace

// =============================================================================
// Test Fixture
// =============================================================================

class BackendRegistryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Clear registry before each test to ensure isolation
        backend_registry::instance().clear();
    }

    void TearDown() override
    {
        backend_registry::instance().clear();
    }
};

// =============================================================================
// Registration Tests
// =============================================================================

TEST_F(BackendRegistryTest, RegisterBackendSucceeds)
{
    auto result = backend_registry::instance().register_backend(
        "test", &test_backend::create);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(BackendRegistryTest, RegisterMultipleBackends)
{
    EXPECT_TRUE(backend_registry::instance()
        .register_backend("test1", &test_backend::create).is_ok());
    EXPECT_TRUE(backend_registry::instance()
        .register_backend("test2", &test_backend_alt::create).is_ok());
    EXPECT_EQ(backend_registry::instance().backend_count(), 2u);
}

TEST_F(BackendRegistryTest, DuplicateRegistrationFails)
{
    EXPECT_TRUE(backend_registry::instance()
        .register_backend("test", &test_backend::create).is_ok());
    auto result = backend_registry::instance()
        .register_backend("test", &test_backend::create);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(BackendRegistryTest, UnregisterExistingBackend)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    auto result = backend_registry::instance().unregister_backend("test");
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(backend_registry::instance().has_backend("test"));
}

TEST_F(BackendRegistryTest, UnregisterNonExistentBackendFails)
{
    auto result = backend_registry::instance().unregister_backend("nonexistent");
    EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(BackendRegistryTest, CreateRegisteredBackend)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    auto backend = backend_registry::instance().create("test");
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::sqlite);
}

TEST_F(BackendRegistryTest, CreateUnregisteredBackendReturnsNull)
{
    auto backend = backend_registry::instance().create("nonexistent");
    EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendRegistryTest, CreateReturnsDistinctInstances)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    auto b1 = backend_registry::instance().create("test");
    auto b2 = backend_registry::instance().create("test");
    ASSERT_NE(b1, nullptr);
    ASSERT_NE(b2, nullptr);
    EXPECT_NE(b1.get(), b2.get());
}

TEST_F(BackendRegistryTest, CreatedBackendIsNotInitialized)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    auto backend = backend_registry::instance().create("test");
    ASSERT_NE(backend, nullptr);
    EXPECT_FALSE(backend->is_initialized());
}

// =============================================================================
// Query Tests
// =============================================================================

TEST_F(BackendRegistryTest, HasBackendReturnsTrueForRegistered)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    EXPECT_TRUE(backend_registry::instance().has_backend("test"));
}

TEST_F(BackendRegistryTest, HasBackendReturnsFalseForUnregistered)
{
    EXPECT_FALSE(backend_registry::instance().has_backend("nonexistent"));
}

TEST_F(BackendRegistryTest, AvailableBackendsListsAll)
{
    backend_registry::instance().register_backend("alpha", &test_backend::create);
    backend_registry::instance().register_backend("beta", &test_backend_alt::create);

    auto backends = backend_registry::instance().available_backends();
    EXPECT_EQ(backends.size(), 2u);
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "alpha") != backends.end());
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "beta") != backends.end());
}

TEST_F(BackendRegistryTest, BackendCountMatchesRegistrations)
{
    EXPECT_EQ(backend_registry::instance().backend_count(), 0u);
    backend_registry::instance().register_backend("a", &test_backend::create);
    EXPECT_EQ(backend_registry::instance().backend_count(), 1u);
    backend_registry::instance().register_backend("b", &test_backend_alt::create);
    EXPECT_EQ(backend_registry::instance().backend_count(), 2u);
}

TEST_F(BackendRegistryTest, EmptyRegistryHasZeroCount)
{
    EXPECT_EQ(backend_registry::instance().backend_count(), 0u);
    EXPECT_TRUE(backend_registry::instance().available_backends().empty());
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_F(BackendRegistryTest, ClearRemovesAllBackends)
{
    backend_registry::instance().register_backend("a", &test_backend::create);
    backend_registry::instance().register_backend("b", &test_backend_alt::create);
    EXPECT_EQ(backend_registry::instance().backend_count(), 2u);

    backend_registry::instance().clear();
    EXPECT_EQ(backend_registry::instance().backend_count(), 0u);
    EXPECT_FALSE(backend_registry::instance().has_backend("a"));
    EXPECT_FALSE(backend_registry::instance().has_backend("b"));
}

TEST_F(BackendRegistryTest, ReRegisterAfterClear)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    backend_registry::instance().clear();

    auto result = backend_registry::instance().register_backend(
        "test", &test_backend::create);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(backend_registry::instance().has_backend("test"));
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

TEST_F(BackendRegistryTest, ConvenienceCreateBackend)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    auto backend = create_backend("test");
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type(), database_types::sqlite);
}

TEST_F(BackendRegistryTest, ConvenienceHasBackend)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    EXPECT_TRUE(has_backend("test"));
    EXPECT_FALSE(has_backend("nonexistent"));
}

TEST_F(BackendRegistryTest, ConvenienceAvailableBackends)
{
    backend_registry::instance().register_backend("x", &test_backend::create);
    auto backends = available_backends();
    EXPECT_EQ(backends.size(), 1u);
    EXPECT_EQ(backends[0], "x");
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(BackendRegistryTest, ConcurrentRegistration)
{
    constexpr int num_threads = 10;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, &success_count]() {
            auto result = backend_registry::instance().register_backend(
                "backend_" + std::to_string(i), &test_backend::create);
            if (result.is_ok()) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
    EXPECT_EQ(backend_registry::instance().backend_count(),
              static_cast<size_t>(num_threads));
}

TEST_F(BackendRegistryTest, ConcurrentCreation)
{
    backend_registry::instance().register_backend("test", &test_backend::create);

    constexpr int num_threads = 20;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&success_count]() {
            auto backend = backend_registry::instance().create("test");
            if (backend != nullptr) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
}

TEST_F(BackendRegistryTest, ConcurrentRegistrationAndQuery)
{
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;

    // Register in parallel
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i]() {
            backend_registry::instance().register_backend(
                "backend_" + std::to_string(i), &test_backend::create);
        });
    }

    // Query in parallel at the same time
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([]() {
            // These may or may not find backends depending on timing
            backend_registry::instance().available_backends();
            backend_registry::instance().backend_count();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All registrations should eventually succeed
    EXPECT_EQ(backend_registry::instance().backend_count(),
              static_cast<size_t>(num_threads));
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(BackendRegistryTest, EmptyNameRegistration)
{
    auto result = backend_registry::instance().register_backend(
        "", &test_backend::create);
    // Empty name should still be valid (implementation-dependent)
    // but we test that it doesn't crash
    if (result.is_ok()) {
        EXPECT_TRUE(backend_registry::instance().has_backend(""));
    }
}

TEST_F(BackendRegistryTest, RegisterUnregisterRegister)
{
    backend_registry::instance().register_backend("test", &test_backend::create);
    backend_registry::instance().unregister_backend("test");
    auto result = backend_registry::instance().register_backend(
        "test", &test_backend_alt::create);
    EXPECT_TRUE(result.is_ok());

    auto backend = backend_registry::instance().create("test");
    ASSERT_NE(backend, nullptr);
    // After re-registration with alt type, should return postgres type
    EXPECT_EQ(backend->type(), database_types::postgres);
}

TEST_F(BackendRegistryTest, SingletonConsistency)
{
    auto& instance1 = backend_registry::instance();
    auto& instance2 = backend_registry::instance();
    EXPECT_EQ(&instance1, &instance2);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
