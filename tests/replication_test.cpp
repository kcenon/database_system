// BSD 3-Clause License
//
// Copyright (c) 2025
// All rights reserved.

/**
 * @file replication_test.cpp
 * @brief Unit tests for replication_manager
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "database/replication/replication_manager.h"
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
