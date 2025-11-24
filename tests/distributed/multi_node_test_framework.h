/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <curl/curl.h>
#include "database/distributed/cluster_manager.h"
#include "database/gateway/database_gateway.h"
#include "database/replication/replication_manager.h"

namespace database::test {

/**
 * @brief Configuration for multi-node test environment
 */
struct multi_node_config {
    int num_primary_nodes = 1;
    int num_replica_nodes = 2;
    bool enable_gateway = true;
    bool enable_replication = true;
    std::chrono::seconds startup_timeout{60};
    bool use_toxiproxy = false;
};

/**
 * @brief Node information
 */
struct node_info {
    std::string id;
    std::string host;
    uint16_t port;
    distributed::node_role role;
    bool is_healthy;
};

/**
 * @brief Base class for distributed system tests
 *
 * Provides infrastructure for testing distributed database features:
 * - Multi-node cluster setup and teardown
 * - Network fault injection via Toxiproxy
 * - Data consistency verification
 * - Health monitoring
 */
class MultiNodeTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Node management
    void start_all_nodes();
    void stop_all_nodes();
    void stop_node(const std::string& node_id);
    void start_node(const std::string& node_id);

    // Network simulation via Toxiproxy
    void partition_node(const std::string& node_id);
    void heal_partition(const std::string& node_id);
    void add_latency(const std::string& node_id, std::chrono::milliseconds latency);
    void remove_latency(const std::string& node_id);
    void add_packet_loss(const std::string& node_id, float probability);
    void remove_packet_loss(const std::string& node_id);

    // Verification utilities
    bool verify_data_consistency();
    bool wait_for_replication_sync(std::chrono::seconds timeout);
    std::map<std::string, int64_t> get_row_counts(const std::string& table);
    bool verify_node_health(const std::string& node_id);

    // Query execution
    core::database_result execute_on_node(const std::string& node_id,
                                          const std::string& query);
    core::database_result execute_on_primary(const std::string& query);
    core::database_result execute_on_replica(const std::string& query);

    // Cluster information
    std::vector<node_info> get_all_nodes() const;
    std::string get_primary_node_id() const;
    std::vector<std::string> get_replica_node_ids() const;
    size_t get_node_count() const;

    // Cluster components
    std::shared_ptr<distributed::cluster_manager> cluster_;
    std::shared_ptr<gateway::database_gateway> gateway_;
    std::shared_ptr<replication::replication_manager> replication_;

    multi_node_config config_;

private:
    // Toxiproxy HTTP API helpers
    static size_t write_callback(void* contents, size_t size, size_t nmemb,
                                 std::string* userp);
    std::string toxiproxy_request(const std::string& method,
                                  const std::string& endpoint,
                                  const std::string& data = "");

    // Node tracking
    std::map<std::string, distributed::node_config> nodes_;
    std::string primary_node_id_;
    std::vector<std::string> replica_node_ids_;

    // Toxiproxy state
    std::string toxiproxy_host_{"localhost"};
    uint16_t toxiproxy_port_{8474};
};

/**
 * @brief Helper class for measuring replication lag
 */
class ReplicationLagMeasurer {
public:
    ReplicationLagMeasurer(MultiNodeTestBase* test_base);

    void insert_test_data(const std::string& table, int id, const std::string& value);
    std::chrono::microseconds measure_lag_to_replica(const std::string& replica_id,
                                                     const std::string& table,
                                                     int id);

    struct lag_stats {
        std::chrono::microseconds min_lag;
        std::chrono::microseconds max_lag;
        std::chrono::microseconds avg_lag;
        std::chrono::microseconds median_lag;
        size_t sample_count;
    };

    lag_stats collect_lag_statistics(size_t sample_count,
                                     const std::string& table = "lag_test");

private:
    MultiNodeTestBase* test_base_;
};

/**
 * @brief Helper class for chaos engineering tests
 */
class ChaosInjector {
public:
    ChaosInjector(MultiNodeTestBase* test_base);

    // Chaos scenarios
    void random_node_failure(std::chrono::seconds duration);
    void network_partition_scenario(const std::vector<std::string>& group1,
                                   const std::vector<std::string>& group2,
                                   std::chrono::seconds duration);
    void latency_spike(const std::string& node_id,
                      std::chrono::milliseconds latency,
                      std::chrono::seconds duration);
    void packet_loss_burst(const std::string& node_id,
                          float loss_probability,
                          std::chrono::seconds duration);

private:
    MultiNodeTestBase* test_base_;
};

} // namespace database::test
