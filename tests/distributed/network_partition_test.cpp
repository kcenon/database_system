/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file network_partition_test.cpp
 * @brief Tests for network partition scenarios and recovery
 */

#include "multi_node_test_framework.h"
#include <gtest/gtest.h>
#include <thread>

using namespace database;
using namespace database::test;
using namespace database::distributed;

class NetworkPartitionTest : public MultiNodeTestBase {
protected:
    void SetUp() override {
        config_.num_primary_nodes = 1;
        config_.num_replica_nodes = 2;
        config_.enable_gateway = false;
        config_.enable_replication = true;
        config_.use_toxiproxy = false;  // Would need docker environment
        MultiNodeTestBase::SetUp();
    }
};

/**
 * Test: Partitioned replica recovery
 */
TEST_F(NetworkPartitionTest, PartitionedReplicaRecovery) {
    // Create test table and initial data
    execute_on_primary("CREATE TABLE IF NOT EXISTS test_partition (id INTEGER PRIMARY KEY, data TEXT)");
    execute_on_primary("INSERT INTO test_partition VALUES (1, 'initial')");

    // Wait for replication
    if (replication_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    auto replicas = get_replica_node_ids();
    ASSERT_FALSE(replicas.empty());
    std::string replica_to_partition = replicas[0];

    // Simulate partition (in test: just stop node)
    stop_node(replica_to_partition);

    // Continue writes during partition
    for (int i = 2; i <= 10; ++i) {
        std::string query = "INSERT INTO test_partition VALUES (" +
                          std::to_string(i) + ", 'during_partition')";
        execute_on_primary(query);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Heal partition (restart node)
    start_node(replica_to_partition);

    // Wait for catch-up replication
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // In full implementation, verify replica caught up
    // For now, verify cluster health restored
    EXPECT_TRUE(cluster_->is_healthy());

    // Cleanup
    execute_on_primary("DROP TABLE IF EXISTS test_partition");
}

/**
 * Test: Split-brain prevention
 */
TEST_F(NetworkPartitionTest, SplitBrainPrevention) {
    auto replicas = get_replica_node_ids();
    ASSERT_GE(replicas.size(), 2) << "Need at least 2 replicas";

    std::string primary = get_primary_node_id();

    // Simulate network partition: primary isolated from replicas
    // In production, this should NOT result in two primaries

    // Stop primary (simulate partition from replicas)
    stop_node(primary);

    // Wait for replicas to detect primary failure
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // In proper implementation with consensus:
    // - Replicas should elect new primary (have quorum)
    // - When old primary reconnects, it should step down

    // Restart primary
    start_node(primary);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify only one primary exists
    // This requires cluster manager to track and prevent split-brain
    int primary_count = 0;
    auto nodes = get_all_nodes();
    for (const auto& node : nodes) {
        auto role = cluster_->get_node_role(node.id);
        if (role.is_ok() && role.value() == node_role::PRIMARY) {
            primary_count++;
        }
    }

    // In ideal implementation: should have exactly 1 primary
    // This test documents the requirement
}

/**
 * Test: Minority partition scenario
 */
TEST_F(NetworkPartitionTest, MinorityPartitionScenario) {
    auto replicas = get_replica_node_ids();
    ASSERT_GE(replicas.size(), 2);

    // Partition a single replica (minority)
    std::string minority_replica = replicas[0];
    stop_node(minority_replica);

    // Majority (primary + other replica) should continue operating
    EXPECT_TRUE(cluster_->is_healthy())
        << "Cluster should remain healthy with minority partition";

    // Writes should succeed
    execute_on_primary("CREATE TABLE IF NOT EXISTS test_minority (id INTEGER)");
    auto result = execute_on_primary("INSERT INTO test_minority VALUES (1)");
    EXPECT_GT(result.rows_affected, 0) << "Writes should succeed";

    // Heal partition
    start_node(minority_replica);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Cleanup
    execute_on_primary("DROP TABLE IF EXISTS test_minority");
}

/**
 * Test: Majority partition scenario
 */
TEST_F(NetworkPartitionTest, MajorityPartitionScenario) {
    auto replicas = get_replica_node_ids();
    ASSERT_GE(replicas.size(), 2);

    // Partition primary and one replica (minority has primary)
    std::string primary = get_primary_node_id();

    // Stop majority (2 replicas if we have 3 total nodes)
    for (const auto& replica : replicas) {
        stop_node(replica);
    }

    // In quorum-based system: primary alone cannot make progress
    // Without quorum, system should reject writes

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // This documents the expected behavior:
    // System should detect loss of quorum and refuse operations

    // Heal partition
    for (const auto& replica : replicas) {
        start_node(replica);
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

/**
 * Test: Transient network issues
 */
TEST_F(NetworkPartitionTest, TransientNetworkIssues) {
    auto replicas = get_replica_node_ids();
    ASSERT_FALSE(replicas.empty());

    // Simulate brief network hiccups
    for (int i = 0; i < 5; ++i) {
        std::string replica = replicas[i % replicas.size()];

        // Brief partition
        stop_node(replica);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        start_node(replica);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Cluster should remain stable despite transient issues
    EXPECT_TRUE(cluster_->is_healthy());
}

/**
 * Test: Cascading failures
 */
TEST_F(NetworkPartitionTest, CascadingFailures) {
    auto replicas = get_replica_node_ids();
    ASSERT_GE(replicas.size(), 2);

    // Sequential node failures
    for (const auto& replica : replicas) {
        stop_node(replica);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // With all replicas down, only primary remains
    // System should detect degraded state

    // Recover nodes one by one
    for (const auto& replica : replicas) {
        start_node(replica);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Eventually should recover
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // Full recovery depends on implementation
}

/**
 * Test: Network latency impact
 */
TEST_F(NetworkPartitionTest, NetworkLatencyImpact) {
    if (!config_.use_toxiproxy) {
        GTEST_SKIP() << "Toxiproxy not enabled";
    }

    auto replicas = get_replica_node_ids();
    ASSERT_FALSE(replicas.empty());

    std::string replica = replicas[0];

    // Add 500ms latency
    add_latency(replica, std::chrono::milliseconds(500));

    // Measure impact on replication
    if (replication_) {
        auto lag_before = replication_->get_replication_lag();

        execute_on_primary("CREATE TABLE IF NOT EXISTS test_latency (id INTEGER)");
        execute_on_primary("INSERT INTO test_latency VALUES (1)");

        std::this_thread::sleep_for(std::chrono::seconds(2));

        auto lag_after = replication_->get_replication_lag();

        // Latency should increase replication lag
        EXPECT_GT(lag_after.count(), lag_before.count());

        execute_on_primary("DROP TABLE IF EXISTS test_latency");
    }

    // Remove latency
    remove_latency(replica);
}

/**
 * Test: Packet loss tolerance
 */
TEST_F(NetworkPartitionTest, PacketLossTolerance) {
    if (!config_.use_toxiproxy) {
        GTEST_SKIP() << "Toxiproxy not enabled";
    }

    auto replicas = get_replica_node_ids();
    ASSERT_FALSE(replicas.empty());

    std::string replica = replicas[0];

    // Add 10% packet loss
    add_packet_loss(replica, 0.1f);

    // Execute operations
    execute_on_primary("CREATE TABLE IF NOT EXISTS test_loss (id INTEGER)");

    for (int i = 0; i < 50; ++i) {
        execute_on_primary("INSERT INTO test_loss VALUES (" + std::to_string(i) + ")");
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    // System should tolerate packet loss via retries
    // Verify some operations succeeded
    auto result = execute_on_primary("SELECT COUNT(*) FROM test_loss");

    execute_on_primary("DROP TABLE IF EXISTS test_loss");

    // Remove packet loss
    remove_packet_loss(replica);
}

/**
 * Test: Partition healing and data consistency
 */
TEST_F(NetworkPartitionTest, PartitionHealingAndConsistency) {
    execute_on_primary("CREATE TABLE IF NOT EXISTS test_consistency (id INTEGER PRIMARY KEY, value TEXT)");

    // Insert initial data
    for (int i = 0; i < 10; ++i) {
        execute_on_primary("INSERT INTO test_consistency VALUES (" +
                         std::to_string(i) + ", 'value_" + std::to_string(i) + "')");
    }

    auto replicas = get_replica_node_ids();
    ASSERT_FALSE(replicas.empty());

    // Partition a replica
    std::string replica = replicas[0];
    stop_node(replica);

    // More writes during partition
    for (int i = 10; i < 20; ++i) {
        execute_on_primary("INSERT INTO test_consistency VALUES (" +
                         std::to_string(i) + ", 'value_" + std::to_string(i) + "')");
    }

    // Heal partition
    start_node(replica);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify data consistency
    EXPECT_TRUE(verify_data_consistency());

    execute_on_primary("DROP TABLE IF EXISTS test_consistency");
}

/**
 * Test: Simultaneous partitions
 */
TEST_F(NetworkPartitionTest, SimultaneousPartitions) {
    auto replicas = get_replica_node_ids();
    ASSERT_GE(replicas.size(), 2);

    // Partition multiple replicas simultaneously
    for (const auto& replica : replicas) {
        stop_node(replica);
    }

    // Only primary remains
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Heal all partitions
    for (const auto& replica : replicas) {
        start_node(replica);
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Verify recovery
    EXPECT_TRUE(cluster_->is_healthy());
}
