/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "cluster_manager.h"
#include <algorithm>
#include <sstream>

namespace database::distributed {

cluster_manager::cluster_manager() {
#ifdef BUILD_WITH_COMMON_SYSTEM
    metrics_ = std::make_shared<monitoring_system::performance_monitor>("cluster_manager");
#endif
}

cluster_manager::~cluster_manager() {
    stop_health_monitoring();
}

result<void> cluster_manager::add_node(const node_config& config) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    // Check if node already exists
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&config](const auto& node) { return node->config.id == config.id; });

    if (it != nodes_.end()) {
        return result<void>::error("Node with ID '" + config.id + "' already exists");
    }

    // Create new node
    auto node = std::make_shared<cluster_node>();
    node->config = config;
    node->stats.node_id = config.id;
    node->stats.last_health_check = std::chrono::steady_clock::now();

    // Create and initialize remote database client
    node->client = std::make_shared<client::remote_database_client>();

    core::connection_config conn_config;
    conn_config.host = config.host;
    conn_config.port = config.port;
    conn_config.database = config.database;
    conn_config.username = config.username;
    conn_config.password = config.password;

    auto init_result = node->client->initialize(conn_config);
    if (init_result.has_error()) {
        return result<void>::error("Failed to initialize node '" + config.id + "': " + init_result.error());
    }

    nodes_.push_back(node);

    return result<void>::ok();
}

result<void> cluster_manager::remove_node(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return result<void>::error("Node with ID '" + node_id + "' not found");
    }

    // Shutdown client
    (*it)->client->shutdown();
    nodes_.erase(it);

    return result<void>::ok();
}

void cluster_manager::set_balancing_strategy(balancing_strategy strategy) {
    strategy_ = strategy;
}

result<core::database_result> cluster_manager::execute_read_query(const std::string& query) {
    auto node = select_read_node();
    if (!node) {
        return result<core::database_result>::error("No healthy nodes available for read query");
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(node->mutex);
        node->stats.query_count++;
    }

#ifdef BUILD_WITH_COMMON_SYSTEM
    if (metrics_) {
        metrics_->record_operation("read_query", 1);
    }
#endif

    auto result = node->client->select_query(query);
    return result;
}

result<uint64_t> cluster_manager::execute_write_query(const std::string& query) {
    auto node = select_primary_node();
    if (!node) {
        return result<uint64_t>::error("No primary node available for write query");
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(node->mutex);
        node->stats.query_count++;
    }

#ifdef BUILD_WITH_COMMON_SYSTEM
    if (metrics_) {
        metrics_->record_operation("write_query", 1);
    }
#endif

    // Determine query type and execute
    std::string query_upper = query;
    std::transform(query_upper.begin(), query_upper.end(), query_upper.begin(), ::toupper);

    if (query_upper.find("INSERT") != std::string::npos) {
        return node->client->insert_query(query);
    } else if (query_upper.find("UPDATE") != std::string::npos) {
        return node->client->update_query(query);
    } else if (query_upper.find("DELETE") != std::string::npos) {
        return node->client->delete_query(query);
    } else {
        return result<uint64_t>::error("Query is not a valid write operation");
    }
}

result<node_stats> cluster_manager::get_node_stats(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return result<node_stats>::error("Node with ID '" + node_id + "' not found");
    }

    std::lock_guard<std::mutex> node_lock((*it)->mutex);
    return result<node_stats>::ok((*it)->stats);
}

std::vector<node_stats> cluster_manager::get_all_node_stats() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    std::vector<node_stats> all_stats;

    for (const auto& node : nodes_) {
        std::lock_guard<std::mutex> node_lock(node->mutex);
        all_stats.push_back(node->stats);
    }

    return all_stats;
}

uint64_t cluster_manager::get_node_connection_count(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return 0;
    }

    std::lock_guard<std::mutex> node_lock((*it)->mutex);
    return (*it)->stats.connection_count;
}

uint64_t cluster_manager::get_node_query_count(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return 0;
    }

    std::lock_guard<std::mutex> node_lock((*it)->mutex);
    return (*it)->stats.query_count;
}

result<node_role> cluster_manager::get_node_role(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return result<node_role>::error("Node with ID '" + node_id + "' not found");
    }

    return result<node_role>::ok((*it)->config.role);
}

result<void> cluster_manager::promote_to_primary(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return result<void>::error("Node with ID '" + node_id + "' not found");
    }

    if ((*it)->config.role == node_role::PRIMARY) {
        return result<void>::error("Node is already primary");
    }

    // Demote current primary to replica
    for (auto& node : nodes_) {
        if (node->config.role == node_role::PRIMARY) {
            node->config.role = node_role::REPLICA;
        }
    }

    // Promote new primary
    (*it)->config.role = node_role::PRIMARY;

    return result<void>::ok();
}

result<void> cluster_manager::start_health_monitoring() {
    if (health_monitoring_active_.load()) {
        return result<void>::error("Health monitoring already running");
    }

    health_monitoring_active_.store(true);
    health_monitoring_thread_ = std::thread(&cluster_manager::health_check_worker, this);

    return result<void>::ok();
}

void cluster_manager::stop_health_monitoring() {
    health_monitoring_active_.store(false);
    if (health_monitoring_thread_.joinable()) {
        health_monitoring_thread_.join();
    }
}

bool cluster_manager::is_healthy() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    bool has_healthy_primary = false;
    bool has_healthy_replica = false;

    for (const auto& node : nodes_) {
        if (node->is_healthy.load()) {
            if (node->config.role == node_role::PRIMARY) {
                has_healthy_primary = true;
            } else if (node->config.role == node_role::REPLICA) {
                has_healthy_replica = true;
            }
        }
    }

    return has_healthy_primary && has_healthy_replica;
}

std::shared_ptr<cluster_manager::cluster_node> cluster_manager::select_read_node() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    // Collect healthy replicas
    std::vector<std::shared_ptr<cluster_node>> healthy_replicas;
    for (auto& node : nodes_) {
        if (node->is_healthy.load() &&
            (node->config.role == node_role::REPLICA || node->config.role == node_role::PRIMARY)) {
            healthy_replicas.push_back(node);
        }
    }

    if (healthy_replicas.empty()) {
        return nullptr;
    }

    // Apply balancing strategy
    switch (strategy_) {
        case balancing_strategy::ROUND_ROBIN: {
            size_t index = round_robin_index_.fetch_add(1) % healthy_replicas.size();
            return healthy_replicas[index];
        }

        case balancing_strategy::LEAST_CONNECTIONS: {
            auto min_node = std::min_element(healthy_replicas.begin(), healthy_replicas.end(),
                [](const auto& a, const auto& b) {
                    return a->stats.connection_count < b->stats.connection_count;
                });
            return *min_node;
        }

        case balancing_strategy::WEIGHTED: {
            // Simple weighted random selection
            uint32_t total_weight = 0;
            for (const auto& node : healthy_replicas) {
                total_weight += node->config.weight;
            }

            if (total_weight == 0) {
                return healthy_replicas[0];
            }

            uint32_t random = std::rand() % total_weight;
            uint32_t cumulative = 0;

            for (const auto& node : healthy_replicas) {
                cumulative += node->config.weight;
                if (random < cumulative) {
                    return node;
                }
            }

            return healthy_replicas[0];
        }

        default:
            return healthy_replicas[0];
    }
}

std::shared_ptr<cluster_manager::cluster_node> cluster_manager::select_primary_node() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    for (auto& node : nodes_) {
        if (node->is_healthy.load() && node->config.role == node_role::PRIMARY) {
            return node;
        }
    }

    return nullptr;
}

void cluster_manager::health_check_worker() {
    while (health_monitoring_active_.load()) {
        std::vector<std::shared_ptr<cluster_node>> nodes_copy;
        {
            std::lock_guard<std::mutex> lock(nodes_mutex_);
            nodes_copy = nodes_;
        }

        for (auto& node : nodes_copy) {
            bool is_healthy = check_node_health(node);

            bool was_healthy = node->is_healthy.exchange(is_healthy);
            if (was_healthy && !is_healthy) {
                handle_node_failure(node->config.id);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool cluster_manager::check_node_health(std::shared_ptr<cluster_node> node) {
    if (!node->client->is_initialized()) {
        return false;
    }

    // Try simple query
    auto result = node->client->select_query("SELECT 1");

    std::lock_guard<std::mutex> lock(node->mutex);
    node->stats.last_health_check = std::chrono::steady_clock::now();
    node->stats.is_healthy = result.is_ok();

    return result.is_ok();
}

void cluster_manager::handle_node_failure(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&node_id](const auto& node) { return node->config.id == node_id; });

    if (it == nodes_.end()) {
        return;
    }

    // If failed node was primary, attempt failover
    if ((*it)->config.role == node_role::PRIMARY) {
        perform_failover();
    }
}

result<void> cluster_manager::perform_failover() {
    // Find healthy replica with highest priority (lowest ID for simplicity)
    std::shared_ptr<cluster_node> best_replica = nullptr;

    for (auto& node : nodes_) {
        if (node->is_healthy.load() && node->config.role == node_role::REPLICA) {
            if (!best_replica || node->config.id < best_replica->config.id) {
                best_replica = node;
            }
        }
    }

    if (!best_replica) {
        return result<void>::error("No healthy replicas available for failover");
    }

    // Promote replica to primary
    best_replica->config.role = node_role::PRIMARY;

    return result<void>::ok();
}

} // namespace database::distributed
