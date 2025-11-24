/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include "../core/database_backend.h"
#include "../core/result.h"
#include "../client/remote_database_client.h"

// Logging/monitoring integration disabled - requires proper CMake setup

namespace database::distributed {

/**
 * @brief Node role in cluster
 */
enum class node_role {
    PRIMARY,   ///< Primary node (accepts writes)
    REPLICA,   ///< Read replica (read-only)
    STANDBY    ///< Standby node (backup)
};

/**
 * @brief Load balancing strategy
 */
enum class balancing_strategy {
    ROUND_ROBIN,        ///< Distribute requests in round-robin fashion
    LEAST_CONNECTIONS,  ///< Route to node with fewest connections
    WEIGHTED            ///< Weighted distribution based on node capacity
};

/**
 * @brief Configuration for a cluster node
 */
struct node_config {
    std::string id;                    ///< Unique node identifier
    std::string host;                  ///< Node hostname or IP
    uint16_t port{5432};               ///< Node port
    node_role role{node_role::REPLICA}; ///< Node role
    uint32_t weight{1};                ///< Weight for weighted balancing (1-100)
    std::chrono::seconds health_check_interval{5}; ///< Health check frequency

    // Connection settings
    std::string database;
    std::string username;
    std::string password;
    bool use_tls{false};
};

/**
 * @brief Cluster node statistics
 */
struct node_stats {
    std::string node_id;
    bool is_healthy{true};
    uint64_t query_count{0};
    uint64_t connection_count{0};
    std::chrono::milliseconds avg_latency{0};
    std::chrono::steady_clock::time_point last_health_check;
};

/**
 * @class cluster_manager
 * @brief Manages a distributed database cluster
 *
 * Features:
 * - Multiple database node management
 * - Load balancing (round robin, least connections, weighted)
 * - Health monitoring and automatic failover
 * - Read/Write separation (Primary-Replica architecture)
 * - Connection pooling via network_system
 *
 * Architecture:
 * - Primary node: Handles all write operations
 * - Replica nodes: Handle read operations (load balanced)
 * - Automatic failover: Promotes replica to primary on failure
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Internal state protected by mutexes
 *
 * Example Usage:
 * @code
 *   auto cluster = std::make_shared<cluster_manager>();
 *
 *   // Add primary node
 *   node_config primary;
 *   primary.id = "primary";
 *   primary.host = "db-primary.example.com";
 *   primary.role = node_role::PRIMARY;
 *   cluster->add_node(primary);
 *
 *   // Add replica nodes
 *   node_config replica1;
 *   replica1.id = "replica1";
 *   replica1.host = "db-replica1.example.com";
 *   replica1.role = node_role::REPLICA;
 *   cluster->add_node(replica1);
 *
 *   // Set balancing strategy
 *   cluster->set_balancing_strategy(balancing_strategy::LEAST_CONNECTIONS);
 *
 *   // Execute queries
 *   auto result = cluster->execute_read_query("SELECT * FROM users");
 *   auto rows_affected = cluster->execute_write_query("INSERT INTO users VALUES (...)");
 * @endcode
 */
class cluster_manager {
public:
    /**
     * @brief Construct a new cluster manager
     */
    cluster_manager();

    /**
     * @brief Destructor - ensures cleanup
     */
    ~cluster_manager();

    /**
     * @brief Add a node to the cluster
     * @param config Node configuration
     * @return result::ok() on success, error on failure
     *
     * Creates a remote_database_client for the node and initializes connection.
     */
    result<void> add_node(const node_config& config);

    /**
     * @brief Remove a node from the cluster
     * @param node_id Node identifier
     * @return result::ok() on success, error if node not found
     */
    result<void> remove_node(const std::string& node_id);

    /**
     * @brief Set load balancing strategy
     * @param strategy Balancing strategy to use
     */
    void set_balancing_strategy(balancing_strategy strategy);

    /**
     * @brief Execute read-only query (routed to replicas)
     * @param query SQL SELECT statement
     * @return Query results or error
     *
     * Automatically routes to healthy replica nodes using current balancing strategy.
     * Falls back to primary if no replicas available.
     */
    result<core::database_result> execute_read_query(const std::string& query);

    /**
     * @brief Execute write query (routed to primary)
     * @param query SQL INSERT/UPDATE/DELETE statement
     * @return Number of rows affected or error
     *
     * Always routes to primary node. Returns error if primary unavailable.
     */
    result<uint64_t> execute_write_query(const std::string& query);

    /**
     * @brief Get node statistics
     * @param node_id Node identifier
     * @return Node statistics or error if node not found
     */
    result<node_stats> get_node_stats(const std::string& node_id) const;

    /**
     * @brief Get all node statistics
     * @return Vector of all node statistics
     */
    std::vector<node_stats> get_all_node_stats() const;

    /**
     * @brief Get node connection count
     * @param node_id Node identifier
     * @return Connection count or 0 if node not found
     */
    uint64_t get_node_connection_count(const std::string& node_id) const;

    /**
     * @brief Get node query count
     * @param node_id Node identifier
     * @return Query count or 0 if node not found
     */
    uint64_t get_node_query_count(const std::string& node_id) const;

    /**
     * @brief Get node role
     * @param node_id Node identifier
     * @return Node role or error if not found
     */
    result<node_role> get_node_role(const std::string& node_id) const;

    /**
     * @brief Promote replica to primary
     * @param node_id Replica node to promote
     * @return result::ok() on success, error on failure
     *
     * Use for manual failover or planned maintenance.
     */
    result<void> promote_to_primary(const std::string& node_id);

    /**
     * @brief Start health monitoring
     * @return result::ok() on success, error if already running
     *
     * Starts background thread that periodically checks node health
     * and performs automatic failover if needed.
     */
    result<void> start_health_monitoring();

    /**
     * @brief Stop health monitoring
     */
    void stop_health_monitoring();

    /**
     * @brief Check if cluster is healthy
     * @return true if at least one primary and one replica are healthy
     */
    bool is_healthy() const;

private:
    /**
     * @brief Internal node representation
     */
    struct cluster_node {
        node_config config;
        std::shared_ptr<client::remote_database_client> client;
        node_stats stats;
        std::atomic<bool> is_healthy{true};
        std::mutex mutex;
    };

    /**
     * @brief Select node for read query using current balancing strategy
     * @return Selected node or nullptr if none available
     */
    std::shared_ptr<cluster_node> select_read_node();

    /**
     * @brief Select primary node for write query
     * @return Primary node or nullptr if not available
     */
    std::shared_ptr<cluster_node> select_primary_node();

    /**
     * @brief Health check worker thread function
     */
    void health_check_worker();

    /**
     * @brief Check health of a specific node
     * @param node Node to check
     * @return true if node is healthy
     */
    bool check_node_health(std::shared_ptr<cluster_node> node);

    /**
     * @brief Handle node failure
     * @param node_id Failed node ID
     */
    void handle_node_failure(const std::string& node_id);

    /**
     * @brief Perform automatic failover
     * @return result::ok() if failover successful
     */
    result<void> perform_failover();

    // Cluster state
    mutable std::mutex nodes_mutex_;
    std::vector<std::shared_ptr<cluster_node>> nodes_;
    balancing_strategy strategy_{balancing_strategy::ROUND_ROBIN};
    std::atomic<size_t> round_robin_index_{0};

    // Health monitoring
    std::atomic<bool> health_monitoring_active_{false};
    std::thread health_monitoring_thread_;

    // Note: Logging/monitoring integration disabled - requires proper CMake setup
};

} // namespace database::distributed
