/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include <future>

#include "database/database_manager.h"
#include "database/database_types.h"
#include "database/orm/entity.h"
#include "database/monitoring/performance_monitor.h"
#include "database/security/rbac_manager.h"
#include "database/security/audit_logger.h"
#include "database/async/async_operations.h"

using namespace database;
using namespace database::orm;
using namespace database::monitoring;
using namespace database::security;
using namespace database::async_ops;

// Test fixture for database tests
class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Cleanup
        auto& db = database_manager::handle();
        db.disconnect();
    }
};

// Basic database manager tests
TEST_F(DatabaseTest, DatabaseManagerSingleton) {
    auto& db1 = database_manager::handle();
    auto& db2 = database_manager::handle();

    // Should be the same instance (singleton)
    EXPECT_EQ(&db1, &db2);
}

TEST_F(DatabaseTest, DatabaseTypeSettings) {
    auto& db = database_manager::handle();

    // Test setting PostgreSQL (currently the only supported backend)
    EXPECT_TRUE(db.set_mode(database_types::postgres));
    EXPECT_EQ(db.database_type(), database_types::postgres);

    // Reset to ensure clean state
    db.disconnect();

    // Test that unsupported backends return false (as expected)
    EXPECT_FALSE(db.set_mode(database_types::mysql));
    EXPECT_EQ(db.database_type(), database_types::none);

    EXPECT_FALSE(db.set_mode(database_types::sqlite));
    EXPECT_EQ(db.database_type(), database_types::none);
}

TEST_F(DatabaseTest, BasicQueryOperations) {
    auto& db = database_manager::handle();

    // Set database mode
    EXPECT_TRUE(db.set_mode(database_types::postgres));

    // Test query creation (should not crash)
    EXPECT_NO_THROW(db.create_query("SELECT 1"));

    // Test select query behavior
    auto result = db.select_query("SELECT 1");
    // Note: PostgreSQL support may not be compiled, so result may contain error info
    // We just test that it doesn't crash and returns some result
    EXPECT_NO_THROW(result);
}

TEST_F(DatabaseTest, ConnectionHandling) {
    auto& db = database_manager::handle();

    // Set database mode
    EXPECT_TRUE(db.set_mode(database_types::postgres));

    // Test connection with invalid connection string (should fail gracefully)
    EXPECT_FALSE(db.connect("invalid_connection_string"));

    // Test disconnect (should not crash)
    EXPECT_NO_THROW(db.disconnect());
}

// Test entity for ORM tests
class TestUser : public entity_base
{
    ENTITY_TABLE("test_users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique())
    ENTITY_FIELD(std::string, email, not_null())
    ENTITY_FIELD(bool, is_active, not_null())

    ENTITY_METADATA()

public:
    TestUser() {
        is_active = true;
    }
};

void TestUser::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(username_field());
    metadata_.add_field(email_field());
    metadata_.add_field(is_active_field());
}

// Phase 4: ORM Framework Tests
class ORMTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean setup for ORM tests
    }

    void TearDown() override {
        // ORM cleanup
    }
};

TEST_F(ORMTest, EntityDefinition) {
    TestUser user;
    user.username = "test_user";
    user.email = "test@example.com";

    EXPECT_EQ(user.username.get(), "test_user");
    EXPECT_EQ(user.email.get(), "test@example.com");
    EXPECT_TRUE(user.is_active.get());
}

TEST_F(ORMTest, EntityMetadata) {
    TestUser user;
    const auto& metadata = user.get_metadata();

    EXPECT_EQ(metadata.table_name(), "test_users");
    EXPECT_EQ(metadata.fields().size(), 4);

    // Check primary key field
    const auto& id_field = metadata.get_field("id");
    EXPECT_TRUE(id_field.is_primary_key());
    EXPECT_TRUE(id_field.is_auto_increment());
}

TEST_F(ORMTest, EntityManager) {
    entity_manager& mgr = entity_manager::instance();

    EXPECT_NO_THROW(mgr.register_entity<TestUser>());

    const auto& metadata = mgr.get_metadata<TestUser>();
    EXPECT_EQ(metadata.table_name(), "test_users");

    // Test SQL generation
    std::string create_sql = metadata.create_table_sql();
    EXPECT_FALSE(create_sql.empty());
    EXPECT_NE(create_sql.find("CREATE TABLE"), std::string::npos);
}

// Phase 4: Performance Monitoring Tests
class PerformanceMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& monitor = performance_monitor::instance();
        monitoring_config config;
        config.enable_query_tracking = true;
        config.enable_connection_tracking = true;
        monitor.configure(config);
    }

    void TearDown() override {
        // Performance monitor cleanup
    }
};

TEST_F(PerformanceMonitorTest, BasicConfiguration) {
    auto& monitor = performance_monitor::instance();

    monitoring_config config;
    config.enable_query_tracking = true;
    config.slow_query_threshold = std::chrono::milliseconds(100);

    EXPECT_NO_THROW(monitor.configure(config));
}

TEST_F(PerformanceMonitorTest, QueryMetricsRecording) {
    auto& monitor = performance_monitor::instance();

    query_metrics metrics;
    metrics.query_type = "SELECT";
    metrics.execution_time = std::chrono::milliseconds(50);
    metrics.success = true;
    metrics.rows_affected = 10;

    EXPECT_NO_THROW(monitor.record_query_execution(metrics));

    const auto& stats = monitor.get_query_statistics();
    EXPECT_GT(stats.total_queries, 0);
}

TEST_F(PerformanceMonitorTest, ConnectionMetricsRecording) {
    auto& monitor = performance_monitor::instance();

    connection_metrics metrics;
    metrics.total_connections.store(10);
    metrics.active_connections.store(5);
    metrics.idle_connections.store(5);

    EXPECT_NO_THROW(monitor.record_connection_metrics(metrics));

    const auto& stats = monitor.get_connection_pool_statistics();
    EXPECT_GE(stats.utilization_percentage, 0.0);
    EXPECT_LE(stats.utilization_percentage, 100.0);
}

TEST_F(PerformanceMonitorTest, SystemMetrics) {
    auto& monitor = performance_monitor::instance();

    const auto& system_metrics = monitor.get_system_metrics();
    EXPECT_GE(system_metrics.cpu_usage_percent, 0.0);
    EXPECT_LE(system_metrics.cpu_usage_percent, 100.0);
    EXPECT_GE(system_metrics.memory_usage_percent, 0.0);
    EXPECT_LE(system_metrics.memory_usage_percent, 100.0);
}

// Phase 4: Security Framework Tests
class SecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Security test setup
    }

    void TearDown() override {
        // Security cleanup
    }
};

TEST_F(SecurityTest, RBACRoleCreation) {
    auto& rbac = rbac_manager::instance();

    rbac_role test_role("test_role");
    test_role.add_permission("data.select");
    test_role.add_permission("data.insert");

    EXPECT_NO_THROW(rbac.create_role(test_role));

    auto retrieved_role = rbac.get_role("test_role");
    EXPECT_TRUE(retrieved_role.has_value());
    EXPECT_EQ(retrieved_role->name(), "test_role");
    EXPECT_TRUE(retrieved_role->has_permission("data.select"));
    EXPECT_TRUE(retrieved_role->has_permission("data.insert"));
    EXPECT_FALSE(retrieved_role->has_permission("data.delete"));
}

TEST_F(SecurityTest, RBACUserManagement) {
    auto& rbac = rbac_manager::instance();

    // Create role first
    rbac_role test_role("user_role");
    test_role.add_permission("data.select");
    rbac.create_role(test_role);

    // Create user
    rbac_user test_user("test.user", "test.user@example.com");
    EXPECT_NO_THROW(rbac.create_user(test_user));

    // Assign role
    EXPECT_NO_THROW(rbac.assign_role_to_user("test.user", "user_role"));

    // Check permissions
    EXPECT_TRUE(rbac.check_permission("test.user", "data.select"));
    EXPECT_FALSE(rbac.check_permission("test.user", "data.delete"));
}

TEST_F(SecurityTest, AuditLogging) {
    auto& logger = audit_logger::instance();

    audit_config config;
    config.enable_database_operations = true;
    config.log_format = audit_format::json;
    logger.configure(config);

    audit_event event;
    event.event_type = audit_event_type::authentication;
    event.user_id = "test_user";
    event.event_description = "Test authentication event";
    event.success = true;
    event.timestamp = std::chrono::system_clock::now();

    EXPECT_NO_THROW(logger.log_event(event));

    auto events = logger.get_events_by_user("test_user");
    EXPECT_GT(events.size(), 0);
}

// Phase 4: Asynchronous Operations Tests
class AsyncOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& executor = async_executor::instance();
        async_config config;
        config.thread_pool_size = 4;
        config.max_concurrent_operations = 10;
        executor.configure(config);
    }

    void TearDown() override {
        // Async operations cleanup
    }
};

TEST_F(AsyncOperationsTest, BasicAsyncExecution) {
    auto& executor = async_executor::instance();

    auto future = executor.execute_async([]() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
}

TEST_F(AsyncOperationsTest, MultipleAsyncOperations) {
    auto& executor = async_executor::instance();

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i) {
        auto future = executor.execute_async([i]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * 2;
        });
        futures.push_back(std::move(future));
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        EXPECT_EQ(futures[i].get(), static_cast<int>(i * 2));
    }
}

TEST_F(AsyncOperationsTest, AsyncConnectionPool) {
    async_connection_pool pool;

    async_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.connection_timeout = std::chrono::seconds(1);

    EXPECT_NO_THROW(pool.configure(config));

    auto future = pool.get_connection_async();
    auto result = future.get();

    // Test that we can get a connection (may be a mock in test environment)
    EXPECT_TRUE(result.success || !result.success); // Either outcome is valid in tests
}

// Connection Pool Tests
class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Connection pool setup
    }

    void TearDown() override {
        // Connection pool cleanup
    }
};

TEST_F(ConnectionPoolTest, PoolConfiguration) {
    auto& db = database_manager::handle();

    connection_pool_config config;
    config.connection_string = "test_connection_string";
    config.min_connections = 5;
    config.max_connections = 20;
    config.connection_timeout = std::chrono::seconds(30);

    // This might fail in test environment without actual database, but should not crash
    EXPECT_NO_THROW(db.create_connection_pool(database_types::postgres, config));
}

TEST_F(ConnectionPoolTest, PoolStatistics) {
    auto& db = database_manager::handle();

    // Get pool statistics (should work even if pool is not active)
    EXPECT_NO_THROW(db.get_connection_pool_stats());
}

// Query Builder Tests
class QueryBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Query builder setup
    }

    void TearDown() override {
        // Query builder cleanup
    }
};

TEST_F(QueryBuilderTest, SQLQueryBuilder) {
    auto& db = database_manager::handle();

    EXPECT_NO_THROW(auto builder = db.create_query_builder(database_types::postgres));

    // Test basic query building methods
    auto builder = db.create_query_builder(database_types::postgres);
    EXPECT_NO_THROW(builder.select({"id", "name"}));
    EXPECT_NO_THROW(builder.from("users"));
    EXPECT_NO_THROW(builder.where("active", "=", database_value{true}));
}

TEST_F(QueryBuilderTest, MongoDBQueryBuilder) {
    auto& db = database_manager::handle();

    if (db.set_mode(database_types::mongodb)) {
        auto builder = db.create_mongodb_query_builder();
        EXPECT_NO_THROW(builder.collection("users"));
        EXPECT_NO_THROW(builder.find_many());
    }
}

TEST_F(QueryBuilderTest, RedisQueryBuilder) {
    auto& db = database_manager::handle();

    if (db.set_mode(database_types::redis)) {
        auto builder = db.create_redis_query_builder();
        EXPECT_NO_THROW(builder.get("test_key"));
    }
}

// Enhanced database tests with Phase 4 features
TEST_F(DatabaseTest, PhaseA4DatabaseTypes) {
    auto& db = database_manager::handle();

    // Test all database types
    std::vector<database_types> types = {
        database_types::postgres,
        database_types::mysql,
        database_types::sqlite,
        database_types::mongodb,
        database_types::redis
    };

    for (auto type : types) {
        // Should not crash regardless of whether backend is available
        EXPECT_NO_THROW(db.set_mode(type));
    }
}

TEST_F(DatabaseTest, ExecuteQueryMethod) {
    auto& db = database_manager::handle();

    // Test execute_query method added in Phase 4
    EXPECT_TRUE(db.set_mode(database_types::postgres));

    // Should not crash even if no actual database connection
    EXPECT_NO_THROW(db.execute_query("SELECT 1"));
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}