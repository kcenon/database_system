/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "multi_node_test_framework.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <thread>

namespace database::test {

// CURL write callback
size_t MultiNodeTestBase::write_callback(void* contents, size_t size,
                                        size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string MultiNodeTestBase::toxiproxy_request(const std::string& method,
                                                 const std::string& endpoint,
                                                 const std::string& data) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string url = "http://" + toxiproxy_host_ + ":" +
                     std::to_string(toxiproxy_port_) + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Toxiproxy request failed: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_cleanup(curl);
    return response;
}

void MultiNodeTestBase::SetUp() {
    // Initialize cluster manager
    cluster_ = std::make_shared<distributed::cluster_manager>();

    // Initialize gateway if enabled
    if (config_.enable_gateway) {
        gateway_ = std::make_shared<gateway::database_gateway>();
    }

    // Initialize replication manager if enabled
    if (config_.enable_replication) {
        replication_ = std::make_shared<replication::replication_manager>();
    }

    // Setup test nodes
    start_all_nodes();
}

void MultiNodeTestBase::TearDown() {
    stop_all_nodes();

    if (gateway_ && gateway_->is_running()) {
        gateway_->stop();
    }

    if (replication_ && replication_->is_active()) {
        replication_->stop_replication();
    }
}

void MultiNodeTestBase::start_all_nodes() {
    // Add primary node
    distributed::node_config primary;
    primary.id = "primary";
    primary.host = "localhost";
    primary.port = 5432;
    primary.role = distributed::node_role::PRIMARY;
    primary.database = "testdb";
    primary.username = "test";
    primary.password = "test";

    auto result = cluster_->add_node(primary);
    if (result.is_ok()) {
        nodes_[primary.id] = primary;
        primary_node_id_ = primary.id;
    }

    // Add replica nodes
    for (int i = 1; i <= config_.num_replica_nodes; ++i) {
        distributed::node_config replica;
        replica.id = "replica" + std::to_string(i);
        replica.host = "localhost";
        replica.port = 5432 + i;
        replica.role = distributed::node_role::REPLICA;
        replica.database = "testdb";
        replica.username = "test";
        replica.password = "test";

        auto replica_result = cluster_->add_node(replica);
        if (replica_result.is_ok()) {
            nodes_[replica.id] = replica;
            replica_node_ids_.push_back(replica.id);
        }
    }

    // Start health monitoring
    cluster_->start_health_monitoring();

    // Wait for cluster to stabilize
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void MultiNodeTestBase::stop_all_nodes() {
    cluster_->stop_health_monitoring();
    nodes_.clear();
    replica_node_ids_.clear();
    primary_node_id_.clear();
}

void MultiNodeTestBase::stop_node(const std::string& node_id) {
    // Remove node from cluster
    auto result = cluster_->remove_node(node_id);
    if (!result.is_ok()) {
        std::cerr << "Failed to stop node: " << node_id << "\n";
    }
}

void MultiNodeTestBase::start_node(const std::string& node_id) {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        auto result = cluster_->add_node(it->second);
        if (!result.is_ok()) {
            std::cerr << "Failed to start node: " << node_id << "\n";
        }
    }
}

void MultiNodeTestBase::partition_node(const std::string& node_id) {
    if (!config_.use_toxiproxy) {
        std::cerr << "Toxiproxy not enabled\n";
        return;
    }

    // Add toxic to simulate network partition
    std::string toxic_data = R"({
        "type": "bandwidth",
        "attributes": {
            "rate": 0
        }
    })";

    toxiproxy_request("POST", "/proxies/" + node_id + "/toxics", toxic_data);
}

void MultiNodeTestBase::heal_partition(const std::string& node_id) {
    if (!config_.use_toxiproxy) {
        return;
    }

    // Remove all toxics
    toxiproxy_request("DELETE", "/proxies/" + node_id + "/toxics/bandwidth");
}

void MultiNodeTestBase::add_latency(const std::string& node_id,
                                   std::chrono::milliseconds latency) {
    if (!config_.use_toxiproxy) {
        return;
    }

    std::string toxic_data = R"({
        "type": "latency",
        "attributes": {
            "latency": )" + std::to_string(latency.count()) + R"(
        }
    })";

    toxiproxy_request("POST", "/proxies/" + node_id + "/toxics", toxic_data);
}

void MultiNodeTestBase::remove_latency(const std::string& node_id) {
    if (!config_.use_toxiproxy) {
        return;
    }

    toxiproxy_request("DELETE", "/proxies/" + node_id + "/toxics/latency");
}

void MultiNodeTestBase::add_packet_loss(const std::string& node_id,
                                       float probability) {
    if (!config_.use_toxiproxy) {
        return;
    }

    std::string toxic_data = R"({
        "type": "loss",
        "attributes": {
            "probability": )" + std::to_string(probability) + R"(
        }
    })";

    toxiproxy_request("POST", "/proxies/" + node_id + "/toxics", toxic_data);
}

void MultiNodeTestBase::remove_packet_loss(const std::string& node_id) {
    if (!config_.use_toxiproxy) {
        return;
    }

    toxiproxy_request("DELETE", "/proxies/" + node_id + "/toxics/loss");
}

bool MultiNodeTestBase::verify_data_consistency() {
    // This is a simplified implementation
    // In a real scenario, you would compare data across all nodes
    return cluster_->is_healthy();
}

bool MultiNodeTestBase::wait_for_replication_sync(std::chrono::seconds timeout) {
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < timeout) {
        if (replication_ && replication_->get_replication_lag() <
            std::chrono::milliseconds(100)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

std::map<std::string, int64_t> MultiNodeTestBase::get_row_counts(
    const std::string& table) {
    std::map<std::string, int64_t> counts;

    // Get count from primary
    auto primary_result = execute_on_primary(
        "SELECT COUNT(*) FROM " + table);
    if (!primary_result.rows.empty() && !primary_result.rows[0].empty()) {
        counts["primary"] = std::stoll(primary_result.rows[0][0]);
    }

    // Get counts from replicas
    for (const auto& replica_id : replica_node_ids_) {
        auto result = execute_on_node(replica_id,
            "SELECT COUNT(*) FROM " + table);
        if (!result.rows.empty() && !result.rows[0].empty()) {
            counts[replica_id] = std::stoll(result.rows[0][0]);
        }
    }

    return counts;
}

bool MultiNodeTestBase::verify_node_health(const std::string& node_id) {
    auto stats_result = cluster_->get_node_stats(node_id);
    return stats_result.is_ok() && stats_result.value().is_healthy;
}

core::database_result MultiNodeTestBase::execute_on_node(
    const std::string& node_id, const std::string& query) {
    // Simplified: execute through cluster manager
    // In reality, you'd need direct node access
    if (query.find("SELECT") != std::string::npos) {
        auto result = cluster_->execute_read_query(query);
        return result.is_ok() ? result.value() : core::database_result{};
    } else {
        auto result = cluster_->execute_write_query(query);
        core::database_result db_result;
        db_result.rows_affected = result.is_ok() ? result.value() : 0;
        return db_result;
    }
}

core::database_result MultiNodeTestBase::execute_on_primary(
    const std::string& query) {
    auto result = cluster_->execute_write_query(query);
    core::database_result db_result;
    db_result.rows_affected = result.is_ok() ? result.value() : 0;
    return db_result;
}

core::database_result MultiNodeTestBase::execute_on_replica(
    const std::string& query) {
    auto result = cluster_->execute_read_query(query);
    return result.is_ok() ? result.value() : core::database_result{};
}

std::vector<node_info> MultiNodeTestBase::get_all_nodes() const {
    std::vector<node_info> result;

    for (const auto& [id, config] : nodes_) {
        node_info info;
        info.id = id;
        info.host = config.host;
        info.port = config.port;
        info.role = config.role;

        auto stats = cluster_->get_node_stats(id);
        info.is_healthy = stats.is_ok() && stats.value().is_healthy;

        result.push_back(info);
    }

    return result;
}

std::string MultiNodeTestBase::get_primary_node_id() const {
    return primary_node_id_;
}

std::vector<std::string> MultiNodeTestBase::get_replica_node_ids() const {
    return replica_node_ids_;
}

size_t MultiNodeTestBase::get_node_count() const {
    return nodes_.size();
}

// ReplicationLagMeasurer implementation

ReplicationLagMeasurer::ReplicationLagMeasurer(MultiNodeTestBase* test_base)
    : test_base_(test_base) {}

void ReplicationLagMeasurer::insert_test_data(const std::string& table,
                                              int id,
                                              const std::string& value) {
    std::string query = "INSERT INTO " + table + " (id, value) VALUES (" +
                       std::to_string(id) + ", '" + value + "')";
    test_base_->execute_on_primary(query);
}

std::chrono::microseconds ReplicationLagMeasurer::measure_lag_to_replica(
    const std::string& replica_id, const std::string& table, int id) {

    auto start = std::chrono::high_resolution_clock::now();

    // Insert on primary
    insert_test_data(table, id, "test_value");

    // Poll replica until data appears
    while (true) {
        auto result = test_base_->execute_on_node(replica_id,
            "SELECT * FROM " + table + " WHERE id = " + std::to_string(id));

        if (!result.rows.empty()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // Timeout after 10 seconds
        if (std::chrono::high_resolution_clock::now() - start >
            std::chrono::seconds(10)) {
            return std::chrono::microseconds::max();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
}

ReplicationLagMeasurer::lag_stats ReplicationLagMeasurer::collect_lag_statistics(
    size_t sample_count, const std::string& table) {

    std::vector<std::chrono::microseconds> samples;
    samples.reserve(sample_count);

    // Create test table
    test_base_->execute_on_primary(
        "CREATE TABLE IF NOT EXISTS " + table + " (id INTEGER PRIMARY KEY, value TEXT)");

    // Collect samples
    for (size_t i = 0; i < sample_count; ++i) {
        auto replicas = test_base_->get_replica_node_ids();
        if (!replicas.empty()) {
            auto lag = measure_lag_to_replica(replicas[0], table, static_cast<int>(i));
            if (lag != std::chrono::microseconds::max()) {
                samples.push_back(lag);
            }
        }
    }

    // Calculate statistics
    lag_stats stats{};
    stats.sample_count = samples.size();

    if (!samples.empty()) {
        stats.min_lag = *std::min_element(samples.begin(), samples.end());
        stats.max_lag = *std::max_element(samples.begin(), samples.end());

        auto sum = std::accumulate(samples.begin(), samples.end(),
                                   std::chrono::microseconds(0));
        stats.avg_lag = sum / samples.size();

        std::sort(samples.begin(), samples.end());
        stats.median_lag = samples[samples.size() / 2];
    }

    // Cleanup
    test_base_->execute_on_primary("DROP TABLE IF EXISTS " + table);

    return stats;
}

// ChaosInjector implementation

ChaosInjector::ChaosInjector(MultiNodeTestBase* test_base)
    : test_base_(test_base) {}

void ChaosInjector::random_node_failure(std::chrono::seconds duration) {
    auto nodes = test_base_->get_all_nodes();
    if (nodes.empty()) return;

    // Pick random node (avoid primary for now)
    auto replicas = test_base_->get_replica_node_ids();
    if (replicas.empty()) return;

    size_t idx = std::rand() % replicas.size();
    std::string node_id = replicas[idx];

    test_base_->stop_node(node_id);
    std::this_thread::sleep_for(duration);
    test_base_->start_node(node_id);
}

void ChaosInjector::network_partition_scenario(
    const std::vector<std::string>& group1,
    const std::vector<std::string>& group2,
    std::chrono::seconds duration) {

    // Partition all nodes in group1 from group2
    for (const auto& node_id : group1) {
        test_base_->partition_node(node_id);
    }

    std::this_thread::sleep_for(duration);

    // Heal partition
    for (const auto& node_id : group1) {
        test_base_->heal_partition(node_id);
    }
}

void ChaosInjector::latency_spike(const std::string& node_id,
                                  std::chrono::milliseconds latency,
                                  std::chrono::seconds duration) {
    test_base_->add_latency(node_id, latency);
    std::this_thread::sleep_for(duration);
    test_base_->remove_latency(node_id);
}

void ChaosInjector::packet_loss_burst(const std::string& node_id,
                                      float loss_probability,
                                      std::chrono::seconds duration) {
    test_base_->add_packet_loss(node_id, loss_probability);
    std::this_thread::sleep_for(duration);
    test_base_->remove_packet_loss(node_id);
}

} // namespace database::test
