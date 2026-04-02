// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_service_registration.cpp
 * @brief Unit tests for database_system's service_container integration.
 *
 * Tests validate that database_system types (IDatabase, database_manager)
 * work correctly with common_system's service_container through the
 * service_registration.h API.
 *
 * Part of kcenon/common_system#369
 */

#include <gtest/gtest.h>

#include <kcenon/common/di/service_container.h>
#include <kcenon/common/interfaces/database_interface.h>
#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM

#include <kcenon/database/di/service_registration.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::common::di;
using namespace kcenon::database::di;
using namespace kcenon::common::interfaces;

/**
 * Test fixture for database DI registration tests
 */
class DatabaseServiceRegistrationTest : public ::testing::Test
{
protected:
    service_container container_;

    void TearDown() override { container_.clear(); }
};

// --- Registration Tests ---

TEST_F(DatabaseServiceRegistrationTest, RegisterDatabaseServices_DefaultConfig_Succeeds)
{
    auto result = register_database_services(container_);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterDatabaseServices_CustomConfig_Succeeds)
{
    database_registration_config config;
    config.db_type = ::database::database_types::sqlite;
    config.lifetime = service_lifetime::singleton;

    auto result = register_database_services(container_, config);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterDatabaseServices_TransientLifetime_Succeeds)
{
    database_registration_config config;
    config.lifetime = service_lifetime::transient;

    auto result = register_database_services(container_, config);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterDatabaseServices_Duplicate_Fails)
{
    auto result1 = register_database_services(container_);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = register_database_services(container_);
    EXPECT_TRUE(result2.is_err());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterAllDatabaseServices_Succeeds)
{
    auto result = register_all_database_services(container_);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterAllDatabaseServices_Duplicate_Fails)
{
    auto result1 = register_all_database_services(container_);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = register_all_database_services(container_);
    EXPECT_TRUE(result2.is_err());
}

// --- Resolution Tests ---

TEST_F(DatabaseServiceRegistrationTest, Resolve_DefaultSingleton_ReturnsSameInstance)
{
    register_database_services(container_);

    auto result1 = container_.resolve<IDatabase>();
    auto result2 = container_.resolve<IDatabase>();

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    // Default lifetime is singleton
    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(DatabaseServiceRegistrationTest, Resolve_Transient_ReturnsDifferentInstances)
{
    database_registration_config config;
    config.lifetime = service_lifetime::transient;

    register_database_services(container_, config);

    auto result1 = container_.resolve<IDatabase>();
    auto result2 = container_.resolve<IDatabase>();

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(DatabaseServiceRegistrationTest, Resolve_ReturnsValidIDatabase)
{
    register_database_services(container_);

    auto result = container_.resolve<IDatabase>();

    ASSERT_TRUE(result.is_ok());
    EXPECT_NE(result.value(), nullptr);

    // Initially not connected
    EXPECT_FALSE(result.value()->is_connected());
}

TEST_F(DatabaseServiceRegistrationTest, ResolveOrNull_Unregistered_ReturnsNull)
{
    auto ptr = container_.resolve_or_null<IDatabase>();

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DatabaseServiceRegistrationTest, ResolveOrNull_Registered_ReturnsInstance)
{
    register_database_services(container_);

    auto ptr = container_.resolve_or_null<IDatabase>();

    EXPECT_NE(ptr, nullptr);
}

// --- Instance Registration Tests ---

TEST_F(DatabaseServiceRegistrationTest, RegisterInstance_Succeeds)
{
    auto manager = std::make_shared<::database::database_manager>(
        std::make_shared<::database::database_context>());

    auto result = register_database_instance(container_, manager);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterInstance_NullManager_Fails)
{
    auto result = register_database_instance(container_, nullptr);

    EXPECT_TRUE(result.is_err());
}

TEST_F(DatabaseServiceRegistrationTest, RegisterInstance_ResolveReturnsSameAdapter)
{
    auto manager = std::make_shared<::database::database_manager>(
        std::make_shared<::database::database_context>());

    register_database_instance(container_, manager);

    auto result1 = container_.resolve<IDatabase>();
    auto result2 = container_.resolve<IDatabase>();

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    // Instance registration is always singleton
    EXPECT_EQ(result1.value(), result2.value());
}

// --- Unregister Tests ---

TEST_F(DatabaseServiceRegistrationTest, UnregisterDatabaseServices_Succeeds)
{
    register_database_services(container_);
    EXPECT_TRUE(container_.is_registered<IDatabase>());

    auto result = unregister_database_services(container_);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(container_.is_registered<IDatabase>());
}

TEST_F(DatabaseServiceRegistrationTest, UnregisterDatabaseServices_NotRegistered_Fails)
{
    auto result = unregister_database_services(container_);
    EXPECT_TRUE(result.is_err());
}

// --- Underlying Manager Utility Tests ---

TEST_F(DatabaseServiceRegistrationTest, GetUnderlyingManager_FactoryRegistered_ReturnsManager)
{
    register_database_services(container_);

    auto db_result = container_.resolve<IDatabase>();
    ASSERT_TRUE(db_result.is_ok());

    auto manager = get_underlying_database_manager(db_result.value());
    EXPECT_NE(manager, nullptr);
}

TEST_F(DatabaseServiceRegistrationTest, GetUnderlyingManager_InstanceRegistered_ReturnsOriginal)
{
    auto original_manager = std::make_shared<::database::database_manager>(
        std::make_shared<::database::database_context>());

    register_database_instance(container_, original_manager);

    auto db_result = container_.resolve<IDatabase>();
    ASSERT_TRUE(db_result.is_ok());

    auto retrieved_manager = get_underlying_database_manager(db_result.value());
    EXPECT_EQ(retrieved_manager, original_manager);
}

// --- Clear Tests ---

TEST_F(DatabaseServiceRegistrationTest, Clear_RemovesRegistration)
{
    register_database_services(container_);
    EXPECT_TRUE(container_.is_registered<IDatabase>());

    container_.clear();

    EXPECT_FALSE(container_.is_registered<IDatabase>());

    auto result = container_.resolve<IDatabase>();
    EXPECT_TRUE(result.is_err());
}

// --- Freeze Tests ---

TEST_F(DatabaseServiceRegistrationTest, Freeze_PreventsRegistration)
{
    register_database_services(container_);
    container_.freeze();

    EXPECT_TRUE(container_.is_frozen());

    // Trying to register another service after freeze should fail
    auto result = container_.register_simple_factory<IDatabase>(
        []() -> std::shared_ptr<IDatabase> { return nullptr; },
        service_lifetime::singleton);

    EXPECT_TRUE(result.is_err());
}

TEST_F(DatabaseServiceRegistrationTest, Freeze_AllowsResolution)
{
    register_database_services(container_);
    container_.freeze();

    auto result = container_.resolve<IDatabase>();
    EXPECT_TRUE(result.is_ok());
    EXPECT_NE(result.value(), nullptr);
}

// --- Thread Safety Tests ---

TEST_F(DatabaseServiceRegistrationTest, ConcurrentResolve_Singleton_ThreadSafe)
{
    register_database_services(container_);

    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<IDatabase>> results(num_threads);

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(
            [this, &results, i]()
            {
                auto result = container_.resolve<IDatabase>();
                if (result.is_ok())
                {
                    results[i] = result.value();
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // All threads should get the same singleton instance
    EXPECT_NE(results[0], nullptr);
    for (int i = 1; i < num_threads; ++i)
    {
        EXPECT_EQ(results[0], results[i]);
    }
}

TEST_F(DatabaseServiceRegistrationTest, ConcurrentResolve_Transient_ThreadSafe)
{
    database_registration_config config;
    config.lifetime = service_lifetime::transient;
    register_database_services(container_, config);

    const int num_threads = 8;
    const int ops_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(
            [this, &success_count, ops_per_thread]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                {
                    auto result = container_.resolve<IDatabase>();
                    if (result.is_ok())
                    {
                        ++success_count;
                    }
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * ops_per_thread);
}

// --- Service Descriptor Tests ---

TEST_F(DatabaseServiceRegistrationTest, RegisteredServices_ReturnsDescriptors)
{
    register_database_services(container_);

    auto services = container_.registered_services();
    EXPECT_GE(services.size(), 1u);
}

// --- Scoped Container Tests ---

TEST_F(DatabaseServiceRegistrationTest, ScopedContainer_InheritsRegistration)
{
    register_database_services(container_);

    auto scope = container_.create_scope();
    ASSERT_NE(scope, nullptr);

    EXPECT_TRUE(scope->is_registered<IDatabase>());

    auto result = scope->resolve<IDatabase>();
    EXPECT_TRUE(result.is_ok());
    EXPECT_NE(result.value(), nullptr);
}

#endif // KCENON_HAS_COMMON_SYSTEM
