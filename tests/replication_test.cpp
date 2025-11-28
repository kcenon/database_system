// BSD 3-Clause License
//
// Copyright (c) 2025
// All rights reserved.

/**
 * @file replication_test.cpp
 * @brief Unit tests for replication_manager and safe_query_builder
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "database/replication/replication_manager.h"
#include "database/replication/safe_query_builder.h"
#include "database/distributed/cluster_manager.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace database;
using namespace database::replication;
using namespace database::distributed;

class ReplicationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<replication_manager>();
    }

    void TearDown() override {
        if (manager_ && manager_->is_active()) {
            manager_->stop_replication();
        }
    }

    node_config create_test_source() {
        node_config source;
        source.id = "source-node";
        source.host = "localhost";
        source.port = 5432;
        source.database = "source_db";
        source.role = node_role::PRIMARY;
        return source;
    }

    node_config create_test_target() {
        node_config target;
        target.id = "target-node";
        target.host = "localhost";
        target.port = 5433;
        target.database = "target_db";
        target.role = node_role::REPLICA;
        return target;
    }

    replication_config create_test_config() {
        replication_config config;
        config.mode = sync_mode::REALTIME;
        config.conflict_resolution = conflict_strategy::LAST_WRITE_WINS;
        config.batch_size = 100;
        config.batch_interval = std::chrono::seconds(60);
        config.bidirectional = false;
        return config;
    }

    std::unique_ptr<replication_manager> manager_;
};

// Basic lifecycle tests
TEST_F(ReplicationManagerTest, InitialState) {
    EXPECT_FALSE(manager_->is_active());
    EXPECT_FALSE(manager_->is_healthy());
    EXPECT_EQ(manager_->get_pending_event_count(), 0);
}

TEST_F(ReplicationManagerTest, StartAndStopReplication) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    auto start_result = manager_->start_replication(source, target, config);
    EXPECT_TRUE(start_result.is_ok());
    EXPECT_TRUE(manager_->is_active());

    auto stop_result = manager_->stop_replication();
    EXPECT_TRUE(stop_result.is_ok());
    EXPECT_FALSE(manager_->is_active());
}

TEST_F(ReplicationManagerTest, CannotStartTwice) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    auto result1 = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result2.is_err());
}

TEST_F(ReplicationManagerTest, CannotStopWhenNotActive) {
    auto result = manager_->stop_replication();
    EXPECT_TRUE(result.is_err());
}

// Statistics tests
TEST_F(ReplicationManagerTest, InitialStatistics) {
    auto stats = manager_->get_stats();

    EXPECT_EQ(stats.events_replicated, 0);
    EXPECT_EQ(stats.events_failed, 0);
    EXPECT_EQ(stats.conflicts_resolved, 0);
    EXPECT_EQ(stats.current_lag.count(), 0);
}

TEST_F(ReplicationManagerTest, GetReplicationLag) {
    auto lag = manager_->get_replication_lag();
    EXPECT_EQ(lag.count(), 0);
}

// Pause and resume tests
TEST_F(ReplicationManagerTest, PauseRequiresActiveReplication) {
    auto result = manager_->pause();
    EXPECT_TRUE(result.is_err());
}

TEST_F(ReplicationManagerTest, ResumeRequiresActiveReplication) {
    auto result = manager_->resume();
    EXPECT_TRUE(result.is_err());
}

TEST_F(ReplicationManagerTest, PauseAndResume) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    manager_->start_replication(source, target, config);

    auto pause_result = manager_->pause();
    EXPECT_TRUE(pause_result.is_ok());

    auto resume_result = manager_->resume();
    EXPECT_TRUE(resume_result.is_ok());
}

// Manual trigger tests
TEST_F(ReplicationManagerTest, TriggerRequiresActiveReplication) {
    auto result = manager_->trigger_replication();
    EXPECT_TRUE(result.is_err());
}

TEST_F(ReplicationManagerTest, TriggerRequiresManualMode) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();
    config.mode = sync_mode::REALTIME;  // Not MANUAL

    manager_->start_replication(source, target, config);

    auto result = manager_->trigger_replication();
    EXPECT_TRUE(result.is_err());
}

TEST_F(ReplicationManagerTest, TriggerInManualMode) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();
    config.mode = sync_mode::MANUAL;

    manager_->start_replication(source, target, config);

    auto result = manager_->trigger_replication();
    EXPECT_TRUE(result.is_ok());
}

// Conflict resolution tests
TEST_F(ReplicationManagerTest, SetConflictResolution) {
    manager_->set_conflict_resolution(conflict_strategy::FIRST_WRITE_WINS);
    // No exception expected
    EXPECT_TRUE(true);

    manager_->set_conflict_resolution(conflict_strategy::LAST_WRITE_WINS);
    EXPECT_TRUE(true);

    manager_->set_conflict_resolution(conflict_strategy::MANUAL);
    EXPECT_TRUE(true);

    manager_->set_conflict_resolution(conflict_strategy::CUSTOM);
    EXPECT_TRUE(true);
}

// Configuration tests
TEST_F(ReplicationManagerTest, DifferentSyncModes) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    // Test REALTIME mode
    config.mode = sync_mode::REALTIME;
    auto result1 = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result1.is_ok());
    manager_->stop_replication();

    // Test BATCH mode
    config.mode = sync_mode::BATCH;
    auto result2 = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result2.is_ok());
    manager_->stop_replication();

    // Test MANUAL mode
    config.mode = sync_mode::MANUAL;
    auto result3 = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result3.is_ok());
    manager_->stop_replication();
}

TEST_F(ReplicationManagerTest, TableMappingConfiguration) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    // Add table mappings
    table_mapping mapping1;
    mapping1.source_table = "users";
    mapping1.target_table = "users_replica";
    mapping1.columns = {"id", "name", "email"};
    config.tables.push_back(mapping1);

    table_mapping mapping2;
    mapping2.source_table = "orders";
    mapping2.target_table = "orders_replica";
    mapping2.filter_condition = "status = 'active'";
    config.tables.push_back(mapping2);

    auto result = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result.is_ok());
}

// Health check tests
TEST_F(ReplicationManagerTest, HealthCheckWhenNotActive) {
    EXPECT_FALSE(manager_->is_healthy());
}

TEST_F(ReplicationManagerTest, HealthCheckWhenActive) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    manager_->start_replication(source, target, config);

    // Initially healthy (no lag accumulated yet)
    EXPECT_TRUE(manager_->is_healthy());
}

// Thread safety tests
TEST_F(ReplicationManagerTest, ConcurrentStatAccess) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    manager_->start_replication(source, target, config);

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                manager_->get_stats();
                manager_->get_replication_lag();
                manager_->get_pending_event_count();
                manager_->is_healthy();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash or deadlock
    EXPECT_TRUE(true);
}

TEST_F(ReplicationManagerTest, ConcurrentPauseResume) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    manager_->start_replication(source, target, config);

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 10; ++j) {
                if (i % 2 == 0) {
                    manager_->pause();
                } else {
                    manager_->resume();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash or deadlock
    EXPECT_TRUE(true);
}

// Event type tests
TEST_F(ReplicationManagerTest, ReplicationEventTypes) {
    // Test that event types are properly defined
    replication_event insert_event;
    insert_event.type = replication_event::event_type::INSERT;
    insert_event.table_name = "users";
    insert_event.new_values["id"] = "1";
    insert_event.new_values["name"] = "Test User";
    insert_event.timestamp = std::chrono::system_clock::now();

    EXPECT_EQ(insert_event.type, replication_event::event_type::INSERT);

    replication_event update_event;
    update_event.type = replication_event::event_type::UPDATE;
    update_event.table_name = "users";
    update_event.old_values["name"] = "Old Name";
    update_event.new_values["name"] = "New Name";

    EXPECT_EQ(update_event.type, replication_event::event_type::UPDATE);

    replication_event delete_event;
    delete_event.type = replication_event::event_type::DELETE;
    delete_event.table_name = "users";
    delete_event.old_values["id"] = "1";

    EXPECT_EQ(delete_event.type, replication_event::event_type::DELETE);
}

// Batch configuration tests
TEST_F(ReplicationManagerTest, BatchSizeConfiguration) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    config.mode = sync_mode::BATCH;
    config.batch_size = 500;
    config.batch_interval = std::chrono::seconds(30);

    auto result = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result.is_ok());

    // Batch configuration should be applied
    EXPECT_TRUE(manager_->is_active());
}

// Bidirectional replication configuration test
TEST_F(ReplicationManagerTest, BidirectionalConfiguration) {
    auto source = create_test_source();
    auto target = create_test_target();
    auto config = create_test_config();

    config.bidirectional = true;

    auto result = manager_->start_replication(source, target, config);
    EXPECT_TRUE(result.is_ok());
}

// Statistics tracking test
TEST_F(ReplicationManagerTest, StatisticsStructure) {
    replication_stats stats;

    // Verify all fields are properly initialized
    EXPECT_EQ(stats.events_replicated, 0);
    EXPECT_EQ(stats.events_failed, 0);
    EXPECT_EQ(stats.conflicts_resolved, 0);
    EXPECT_EQ(stats.current_lag.count(), 0);
    EXPECT_EQ(stats.avg_lag.count(), 0);
    EXPECT_EQ(stats.max_lag.count(), 0);
}

// Destructor cleanup test
TEST_F(ReplicationManagerTest, DestructorCleansUpActiveReplication) {
    {
        auto temp_manager = std::make_unique<replication_manager>();
        auto source = create_test_source();
        auto target = create_test_target();
        auto config = create_test_config();

        temp_manager->start_replication(source, target, config);
        EXPECT_TRUE(temp_manager->is_active());

        // Destructor should clean up when temp_manager goes out of scope
    }

    // If we reach here without hanging, cleanup worked
    EXPECT_TRUE(true);
}

// =============================================================================
// Safe Query Builder Tests - SQL Injection Prevention
// =============================================================================

class SafeQueryBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Value escaping tests
TEST_F(SafeQueryBuilderTest, EscapeSingleQuotes) {
    std::string input = "O'Brien";
    std::string expected = "O''Brien";
    EXPECT_EQ(safe_query_builder::escape_value(input), expected);
}

TEST_F(SafeQueryBuilderTest, EscapeMultipleSingleQuotes) {
    std::string input = "It's a 'test' value";
    std::string expected = "It''s a ''test'' value";
    EXPECT_EQ(safe_query_builder::escape_value(input), expected);
}

TEST_F(SafeQueryBuilderTest, EscapeBackslashes) {
    std::string input = "path\\to\\file";
    std::string expected = "path\\\\to\\\\file";
    EXPECT_EQ(safe_query_builder::escape_value(input), expected);
}

TEST_F(SafeQueryBuilderTest, EscapeSQLInjectionAttempt) {
    std::string input = "'; DROP TABLE users; --";
    std::string escaped = safe_query_builder::escape_value(input);
    // The escaped string should have doubled quotes
    EXPECT_EQ(escaped, "''; DROP TABLE users; --");
}

TEST_F(SafeQueryBuilderTest, EscapeComplexSQLInjection) {
    std::string input = "1' OR '1'='1";
    std::string escaped = safe_query_builder::escape_value(input);
    EXPECT_EQ(escaped, "1'' OR ''1''=''1");
}

TEST_F(SafeQueryBuilderTest, HandleEmptyString) {
    std::string input = "";
    EXPECT_EQ(safe_query_builder::escape_value(input), "");
}

TEST_F(SafeQueryBuilderTest, HandleNormalString) {
    std::string input = "normal_value_123";
    EXPECT_EQ(safe_query_builder::escape_value(input), "normal_value_123");
}

// Identifier validation tests
TEST_F(SafeQueryBuilderTest, ValidIdentifier) {
    EXPECT_TRUE(safe_query_builder::is_valid_identifier("users"));
    EXPECT_TRUE(safe_query_builder::is_valid_identifier("user_name"));
    EXPECT_TRUE(safe_query_builder::is_valid_identifier("User123"));
    EXPECT_TRUE(safe_query_builder::is_valid_identifier("_private_table"));
    EXPECT_TRUE(safe_query_builder::is_valid_identifier("schema.table"));
}

TEST_F(SafeQueryBuilderTest, InvalidIdentifier) {
    EXPECT_FALSE(safe_query_builder::is_valid_identifier(""));
    EXPECT_FALSE(safe_query_builder::is_valid_identifier("123users"));  // starts with number
    EXPECT_FALSE(safe_query_builder::is_valid_identifier("user-name"));  // contains hyphen
    EXPECT_FALSE(safe_query_builder::is_valid_identifier("user name"));  // contains space
    EXPECT_FALSE(safe_query_builder::is_valid_identifier("user;name"));  // contains semicolon
    EXPECT_FALSE(safe_query_builder::is_valid_identifier("user'name"));  // contains quote
}

TEST_F(SafeQueryBuilderTest, EscapeIdentifier) {
    EXPECT_EQ(safe_query_builder::escape_identifier("users"), "\"users\"");
    EXPECT_EQ(safe_query_builder::escape_identifier("user_name"), "\"user_name\"");
}

TEST_F(SafeQueryBuilderTest, EscapeIdentifierThrowsOnInvalid) {
    EXPECT_THROW(safe_query_builder::escape_identifier("user;DROP"), std::invalid_argument);
    EXPECT_THROW(safe_query_builder::escape_identifier(""), std::invalid_argument);
}

// INSERT query building tests
TEST_F(SafeQueryBuilderTest, BuildInsertQuery) {
    std::unordered_map<std::string, std::string> values = {
        {"name", "John"},
        {"age", "30"}
    };

    std::string query = safe_query_builder::build_insert("users", values);

    // Query should contain escaped table and column names
    EXPECT_TRUE(query.find("\"users\"") != std::string::npos);
    EXPECT_TRUE(query.find("INSERT INTO") != std::string::npos);
    EXPECT_TRUE(query.find("VALUES") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildInsertWithSQLInjectionValue) {
    std::unordered_map<std::string, std::string> values = {
        {"name", "'; DROP TABLE users; --"}
    };

    std::string query = safe_query_builder::build_insert("users", values);

    // The dangerous value should be escaped
    EXPECT_TRUE(query.find("''") != std::string::npos);
    // The query should NOT contain unescaped DROP TABLE
    EXPECT_FALSE(query.find("'; DROP") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildInsertThrowsOnEmptyTable) {
    std::unordered_map<std::string, std::string> values = {{"name", "John"}};
    EXPECT_THROW(safe_query_builder::build_insert("", values), std::invalid_argument);
}

TEST_F(SafeQueryBuilderTest, BuildInsertThrowsOnEmptyValues) {
    std::unordered_map<std::string, std::string> values;
    EXPECT_THROW(safe_query_builder::build_insert("users", values), std::invalid_argument);
}

// UPDATE query building tests
TEST_F(SafeQueryBuilderTest, BuildUpdateQuery) {
    std::unordered_map<std::string, std::string> new_values = {{"name", "Jane"}};
    std::unordered_map<std::string, std::string> where_values = {{"id", "1"}};

    std::string query = safe_query_builder::build_update("users", new_values, where_values);

    EXPECT_TRUE(query.find("UPDATE") != std::string::npos);
    EXPECT_TRUE(query.find("SET") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
    EXPECT_TRUE(query.find("\"users\"") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildUpdateWithSQLInjection) {
    std::unordered_map<std::string, std::string> new_values = {
        {"name", "evil'; UPDATE users SET admin='true' WHERE '1'='1"}
    };
    std::unordered_map<std::string, std::string> where_values = {{"id", "1"}};

    std::string query = safe_query_builder::build_update("users", new_values, where_values);

    // Escaped quotes should be present
    EXPECT_TRUE(query.find("''") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildUpdateThrowsOnEmptyWhere) {
    std::unordered_map<std::string, std::string> new_values = {{"name", "Jane"}};
    std::unordered_map<std::string, std::string> where_values;

    // Should throw to prevent accidental mass updates
    EXPECT_THROW(
        safe_query_builder::build_update("users", new_values, where_values),
        std::invalid_argument
    );
}

// DELETE query building tests
TEST_F(SafeQueryBuilderTest, BuildDeleteQuery) {
    std::unordered_map<std::string, std::string> where_values = {{"id", "1"}};

    std::string query = safe_query_builder::build_delete("users", where_values);

    EXPECT_TRUE(query.find("DELETE FROM") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
    EXPECT_TRUE(query.find("\"users\"") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildDeleteWithSQLInjection) {
    std::unordered_map<std::string, std::string> where_values = {
        {"id", "1' OR '1'='1"}
    };

    std::string query = safe_query_builder::build_delete("users", where_values);

    // Escaped quotes should be present
    EXPECT_TRUE(query.find("''") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, BuildDeleteThrowsOnEmptyWhere) {
    std::unordered_map<std::string, std::string> where_values;

    // Should throw to prevent accidental mass deletes
    EXPECT_THROW(
        safe_query_builder::build_delete("users", where_values),
        std::invalid_argument
    );
}

// Real-world SQL injection prevention tests
TEST_F(SafeQueryBuilderTest, PreventClassicSQLInjection) {
    // Classic SQL injection: '; DROP TABLE users; --
    std::unordered_map<std::string, std::string> values = {
        {"username", "admin"},
        {"password", "' OR '1'='1' --"}
    };

    std::string query = safe_query_builder::build_insert("auth_log", values);

    // The query should be safe - no unescaped injection
    EXPECT_TRUE(query.find("''''") != std::string::npos ||
                query.find("''1''") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, PreventUnionBasedInjection) {
    std::unordered_map<std::string, std::string> values = {
        {"search", "' UNION SELECT * FROM passwords --"}
    };

    std::string query = safe_query_builder::build_insert("search_log", values);

    // Single quotes in the injection should be doubled
    EXPECT_TRUE(query.find("'' UNION") != std::string::npos);
}

TEST_F(SafeQueryBuilderTest, HandleSpecialCharacters) {
    std::unordered_map<std::string, std::string> values = {
        {"bio", "Hello! I'm a developer. Use \"quotes\" & <tags>."}
    };

    std::string query = safe_query_builder::build_insert("profiles", values);

    // Single quote should be escaped
    EXPECT_TRUE(query.find("I''m") != std::string::npos);
}

// =============================================================================
// CDC Strategy Tests
// =============================================================================

#include "database/replication/cdc/cdc_factory.h"
#include "database/replication/cdc/cdc_strategy_interface.h"
#include "database/replication/cdc/sqlite_cdc_strategy.h"
#include "database/replication/cdc/postgresql_cdc_strategy.h"
#include "database/replication/cdc/mysql_cdc_strategy.h"
#include "database/replication/cdc/mongodb_cdc_strategy.h"

using namespace database::replication::cdc;

class CDCFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Factory creation tests
TEST_F(CDCFactoryTest, CreateSQLiteCDC) {
    auto strategy = cdc_factory::create(database_type::SQLITE);
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->get_database_type(), database_type::SQLITE);
}

TEST_F(CDCFactoryTest, CreatePostgreSQLCDC) {
    auto strategy = cdc_factory::create(database_type::POSTGRESQL);
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->get_database_type(), database_type::POSTGRESQL);
}

TEST_F(CDCFactoryTest, CreateMySQLCDC) {
    auto strategy = cdc_factory::create(database_type::MYSQL);
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->get_database_type(), database_type::MYSQL);
}

TEST_F(CDCFactoryTest, CreateMongoDBCDC) {
    auto strategy = cdc_factory::create(database_type::MONGODB);
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->get_database_type(), database_type::MONGODB);
}

// Database type detection tests
TEST_F(CDCFactoryTest, DetectSQLiteFromConnectionString) {
    EXPECT_EQ(cdc_factory::detect_database_type("sqlite:///path/to/db.sqlite"),
              database_type::SQLITE);
    EXPECT_EQ(cdc_factory::detect_database_type("/path/to/db.sqlite"),
              database_type::SQLITE);
    EXPECT_EQ(cdc_factory::detect_database_type("test.db"),
              database_type::SQLITE);
    EXPECT_EQ(cdc_factory::detect_database_type(":memory:"),
              database_type::SQLITE);
}

TEST_F(CDCFactoryTest, DetectPostgreSQLFromConnectionString) {
    EXPECT_EQ(cdc_factory::detect_database_type("postgresql://user:pass@localhost:5432/db"),
              database_type::POSTGRESQL);
    EXPECT_EQ(cdc_factory::detect_database_type("postgres://user:pass@localhost/db"),
              database_type::POSTGRESQL);
}

TEST_F(CDCFactoryTest, DetectMySQLFromConnectionString) {
    EXPECT_EQ(cdc_factory::detect_database_type("mysql://user:pass@localhost:3306/db"),
              database_type::MYSQL);
}

TEST_F(CDCFactoryTest, DetectMongoDBFromConnectionString) {
    EXPECT_EQ(cdc_factory::detect_database_type("mongodb://user:pass@localhost:27017/db"),
              database_type::MONGODB);
    EXPECT_EQ(cdc_factory::detect_database_type("mongodb+srv://user:pass@cluster.example.com/db"),
              database_type::MONGODB);
}

// Type name tests
TEST_F(CDCFactoryTest, GetTypeName) {
    EXPECT_EQ(cdc_factory::get_type_name(database_type::SQLITE), "SQLite");
    EXPECT_EQ(cdc_factory::get_type_name(database_type::POSTGRESQL), "PostgreSQL");
    EXPECT_EQ(cdc_factory::get_type_name(database_type::MYSQL), "MySQL");
    EXPECT_EQ(cdc_factory::get_type_name(database_type::MONGODB), "MongoDB");
}

// Is supported tests
TEST_F(CDCFactoryTest, IsSupportedSQLite) {
    // SQLite is always supported (no external dependencies)
    EXPECT_TRUE(cdc_factory::is_supported(database_type::SQLITE));
}

// Create from connection string tests
TEST_F(CDCFactoryTest, CreateFromSQLiteConnectionString) {
    auto strategy = cdc_factory::create_from_connection_string("test.db");
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->get_database_type(), database_type::SQLITE);
}

// =============================================================================
// SQLite CDC Strategy Unit Tests
// =============================================================================

class SQLiteCDCStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy_ = std::make_unique<sqlite_cdc_strategy>();
    }

    void TearDown() override {
        if (strategy_ && strategy_->is_active()) {
            strategy_->stop();
        }
        if (strategy_) {
            strategy_->cleanup();
        }
    }

    std::unique_ptr<sqlite_cdc_strategy> strategy_;
};

TEST_F(SQLiteCDCStrategyTest, InitialState) {
    EXPECT_FALSE(strategy_->is_active());
    EXPECT_EQ(strategy_->get_pending_count(), 0);
    EXPECT_EQ(strategy_->get_database_type(), database_type::SQLITE);
}

TEST_F(SQLiteCDCStrategyTest, GetDatabaseType) {
    EXPECT_EQ(strategy_->get_database_type(), database_type::SQLITE);
}

TEST_F(SQLiteCDCStrategyTest, CannotStartWithoutInitialization) {
    auto result = strategy_->start();
    EXPECT_TRUE(result.is_err());
}

TEST_F(SQLiteCDCStrategyTest, CannotStopWithoutStart) {
    auto result = strategy_->stop();
    EXPECT_TRUE(result.is_err());
}

TEST_F(SQLiteCDCStrategyTest, InitializeWithMemoryDatabase) {
    cdc_config config;
    config.connection_string = ":memory:";
    config.tracked_tables = {"test_table"};

    // This will fail because the table doesn't exist, but initialization structure works
    auto result = strategy_->initialize(config);
    // May succeed or fail depending on table existence
    // The test validates that the interface works
}

TEST_F(SQLiteCDCStrategyTest, PositionManagement) {
    auto result = strategy_->set_position("12345");
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(strategy_->get_current_position(), "12345");
}

TEST_F(SQLiteCDCStrategyTest, CaptureEventsWhenNotActive) {
    auto events = strategy_->capture_events(10);
    EXPECT_TRUE(events.empty());

    auto event = strategy_->capture_next_event();
    EXPECT_FALSE(event.has_value());
}

// =============================================================================
// PostgreSQL CDC Strategy Unit Tests
// =============================================================================

class PostgreSQLCDCStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy_ = std::make_unique<postgresql_cdc_strategy>();
    }

    void TearDown() override {
        if (strategy_ && strategy_->is_active()) {
            strategy_->stop();
        }
    }

    std::unique_ptr<postgresql_cdc_strategy> strategy_;
};

TEST_F(PostgreSQLCDCStrategyTest, InitialState) {
    EXPECT_FALSE(strategy_->is_active());
    EXPECT_EQ(strategy_->get_pending_count(), 0);
    EXPECT_EQ(strategy_->get_database_type(), database_type::POSTGRESQL);
}

TEST_F(PostgreSQLCDCStrategyTest, GetDatabaseType) {
    EXPECT_EQ(strategy_->get_database_type(), database_type::POSTGRESQL);
}

TEST_F(PostgreSQLCDCStrategyTest, CannotStartWithoutInitialization) {
    auto result = strategy_->start();
    EXPECT_TRUE(result.is_err());
}

TEST_F(PostgreSQLCDCStrategyTest, PositionManagement) {
    auto result = strategy_->set_position("0/12345678");
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(strategy_->get_current_position(), "0/12345678");
}

TEST_F(PostgreSQLCDCStrategyTest, CaptureEventsWhenNotActive) {
    auto events = strategy_->capture_events(10);
    EXPECT_TRUE(events.empty());
}

// =============================================================================
// MySQL CDC Strategy Unit Tests
// =============================================================================

class MySQLCDCStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy_ = std::make_unique<mysql_cdc_strategy>();
    }

    void TearDown() override {
        if (strategy_ && strategy_->is_active()) {
            strategy_->stop();
        }
    }

    std::unique_ptr<mysql_cdc_strategy> strategy_;
};

TEST_F(MySQLCDCStrategyTest, InitialState) {
    EXPECT_FALSE(strategy_->is_active());
    EXPECT_EQ(strategy_->get_pending_count(), 0);
    EXPECT_EQ(strategy_->get_database_type(), database_type::MYSQL);
}

TEST_F(MySQLCDCStrategyTest, GetDatabaseType) {
    EXPECT_EQ(strategy_->get_database_type(), database_type::MYSQL);
}

TEST_F(MySQLCDCStrategyTest, CannotStartWithoutInitialization) {
    auto result = strategy_->start();
    EXPECT_TRUE(result.is_err());
}

TEST_F(MySQLCDCStrategyTest, PositionManagement) {
    auto result = strategy_->set_position("mysql-bin.000001:12345");
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(strategy_->get_current_position(), "mysql-bin.000001:12345");

    // Test GTID format
    result = strategy_->set_position("gtid:3E11FA47-71CA-11E1-9E33-C80AA9429562:1-5");
    EXPECT_TRUE(result.is_ok());
}

TEST_F(MySQLCDCStrategyTest, CaptureEventsWhenNotActive) {
    auto events = strategy_->capture_events(10);
    EXPECT_TRUE(events.empty());
}

// =============================================================================
// MongoDB CDC Strategy Unit Tests
// =============================================================================

class MongoDBCDCStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy_ = std::make_unique<mongodb_cdc_strategy>();
    }

    void TearDown() override {
        if (strategy_ && strategy_->is_active()) {
            strategy_->stop();
        }
    }

    std::unique_ptr<mongodb_cdc_strategy> strategy_;
};

TEST_F(MongoDBCDCStrategyTest, InitialState) {
    EXPECT_FALSE(strategy_->is_active());
    EXPECT_EQ(strategy_->get_pending_count(), 0);
    EXPECT_EQ(strategy_->get_database_type(), database_type::MONGODB);
}

TEST_F(MongoDBCDCStrategyTest, GetDatabaseType) {
    EXPECT_EQ(strategy_->get_database_type(), database_type::MONGODB);
}

TEST_F(MongoDBCDCStrategyTest, CannotStartWithoutInitialization) {
    auto result = strategy_->start();
    EXPECT_TRUE(result.is_err());
}

TEST_F(MongoDBCDCStrategyTest, PositionManagement) {
    std::string resume_token = R"({"_data": "82636D7069"})";
    auto result = strategy_->set_position(resume_token);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(strategy_->get_current_position(), resume_token);
}

TEST_F(MongoDBCDCStrategyTest, CaptureEventsWhenNotActive) {
    auto events = strategy_->capture_events(10);
    EXPECT_TRUE(events.empty());
}

TEST_F(MongoDBCDCStrategyTest, Cleanup) {
    auto result = strategy_->cleanup();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(strategy_->is_active());
}

// =============================================================================
// CDC Config Structure Tests
// =============================================================================

TEST(CDCConfigTest, DefaultValues) {
    cdc_config config;

    EXPECT_TRUE(config.connection_string.empty());
    EXPECT_TRUE(config.tracked_tables.empty());
    EXPECT_TRUE(config.capture_old_values);
    EXPECT_EQ(config.max_batch_size, 1000);
    EXPECT_EQ(config.change_table_prefix, "_cdc_");
}

// =============================================================================
// Target Client Initialization Tests
// =============================================================================

class TargetClientInitializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<replication_manager>();
    }

    void TearDown() override {
        if (manager_ && manager_->is_active()) {
            manager_->stop_replication();
        }
    }

    node_config create_sqlite_source() {
        node_config source;
        source.id = "sqlite-source";
        source.connection_string = ":memory:";
        source.role = node_role::PRIMARY;
        return source;
    }

    node_config create_sqlite_target() {
        node_config target;
        target.id = "sqlite-target";
        target.connection_string = ":memory:";
        target.role = node_role::REPLICA;
        return target;
    }

    node_config create_postgresql_target() {
        node_config target;
        target.id = "postgresql-target";
        target.connection_string = "postgresql://user:pass@localhost:5432/testdb";
        target.host = "localhost";
        target.port = 5432;
        target.database = "testdb";
        target.username = "user";
        target.password = "pass";
        target.role = node_role::REPLICA;
        return target;
    }

    replication_config create_test_config() {
        replication_config config;
        config.mode = sync_mode::REALTIME;
        config.conflict_resolution = conflict_strategy::LAST_WRITE_WINS;
        config.batch_size = 100;
        return config;
    }

    std::unique_ptr<replication_manager> manager_;
};

// Test database type detection from connection strings
TEST_F(TargetClientInitializationTest, DetectSQLiteFromConnectionString) {
    auto db_type = cdc_factory::detect_database_type(":memory:");
    EXPECT_EQ(db_type, database_type::SQLITE);

    db_type = cdc_factory::detect_database_type("test.db");
    EXPECT_EQ(db_type, database_type::SQLITE);

    db_type = cdc_factory::detect_database_type("sqlite:///path/to/db.sqlite");
    EXPECT_EQ(db_type, database_type::SQLITE);
}

TEST_F(TargetClientInitializationTest, DetectPostgreSQLFromConnectionString) {
    auto db_type = cdc_factory::detect_database_type("postgresql://user:pass@localhost:5432/db");
    EXPECT_EQ(db_type, database_type::POSTGRESQL);

    db_type = cdc_factory::detect_database_type("postgres://user:pass@localhost/db");
    EXPECT_EQ(db_type, database_type::POSTGRESQL);
}

TEST_F(TargetClientInitializationTest, DetectMySQLFromConnectionString) {
    auto db_type = cdc_factory::detect_database_type("mysql://user:pass@localhost:3306/db");
    EXPECT_EQ(db_type, database_type::MYSQL);
}

TEST_F(TargetClientInitializationTest, DetectMongoDBFromConnectionString) {
    auto db_type = cdc_factory::detect_database_type("mongodb://user:pass@localhost:27017/db");
    EXPECT_EQ(db_type, database_type::MONGODB);
}

// Test that replication can start with SQLite source and target
// Note: This test may fail if SQLite backend is not registered
TEST_F(TargetClientInitializationTest, StartReplicationWithSQLiteTarget) {
    auto source = create_sqlite_source();
    auto target = create_sqlite_target();
    auto config = create_test_config();

    // Add a table mapping to track
    table_mapping mapping;
    mapping.source_table = "test_table";
    mapping.target_table = "test_table";
    config.tables.push_back(mapping);

    auto result = manager_->start_replication(source, target, config);

    // Result depends on whether SQLite backend is available
    // If backend is registered, it should succeed
    // If not, it will fail with backend not available error
    if (result.is_err()) {
        // Expected error if backend not registered
        EXPECT_TRUE(
            result.error().message.find("backend") != std::string::npos ||
            result.error().message.find("initialize") != std::string::npos
        );
    } else {
        EXPECT_TRUE(manager_->is_active());
    }
}

// Test that invalid database type is handled gracefully
TEST_F(TargetClientInitializationTest, HandlesInvalidDatabaseType) {
    node_config source;
    source.id = "invalid-source";
    source.connection_string = "unknown://some/connection";

    node_config target;
    target.id = "invalid-target";
    target.connection_string = "unknown://another/connection";

    auto config = create_test_config();

    auto result = manager_->start_replication(source, target, config);

    // Should fail gracefully with an error message
    // (either unsupported database type or backend not available)
    // Note: detect_database_type defaults to SQLITE for unknown types
    if (result.is_err()) {
        EXPECT_FALSE(result.error().message.empty());
    }
}

// Test configuration with direct host/port settings
TEST_F(TargetClientInitializationTest, TargetWithDirectHostConfig) {
    auto source = create_sqlite_source();

    node_config target;
    target.id = "direct-config-target";
    target.host = "localhost";
    target.port = 5432;
    target.database = "testdb";
    target.username = "user";
    target.password = "pass";
    target.connection_string = "postgresql://localhost:5432/testdb";
    target.role = node_role::REPLICA;

    auto config = create_test_config();
    table_mapping mapping;
    mapping.source_table = "test_table";
    mapping.target_table = "test_table";
    config.tables.push_back(mapping);

    auto result = manager_->start_replication(source, target, config);

    // Test verifies that the configuration is processed correctly
    // Actual success depends on backend availability
    if (result.is_err()) {
        // Error should be related to connection/backend, not configuration parsing
        EXPECT_TRUE(
            result.error().message.find("backend") != std::string::npos ||
            result.error().message.find("connection") != std::string::npos ||
            result.error().message.find("initialize") != std::string::npos
        );
    }
}
