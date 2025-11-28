/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "../core/database_backend.h"
#include "../core/result.h"
#include "../distributed/cluster_manager.h"
#include "cdc/cdc_strategy_interface.h"

// Logging/monitoring integration
#include "../integrated/adapters/logger_adapter.h"
#include "../integrated/adapters/monitoring_adapter.h"

namespace database::replication {

/**
 * @brief Replication sync mode
 */
enum class sync_mode {
    REALTIME,       ///< Real-time change replication
    BATCH,          ///< Batch replication at intervals
    MANUAL          ///< Manual replication trigger
};

/**
 * @brief Conflict resolution strategy
 */
enum class conflict_strategy {
    LAST_WRITE_WINS,    ///< Use timestamp to resolve conflicts
    FIRST_WRITE_WINS,   ///< Keep first write, reject later writes
    MANUAL,             ///< Require manual conflict resolution
    CUSTOM              ///< Use custom conflict resolution function
};

/**
 * @brief Table mapping for replication
 */
struct table_mapping {
    std::string source_table;
    std::string target_table;
    std::vector<std::string> columns;  ///< Columns to replicate (empty = all)
    std::string filter_condition;      ///< WHERE condition for filtering rows
};

/**
 * @brief Replication configuration
 */
struct replication_config {
    std::vector<table_mapping> tables;          ///< Table mappings
    sync_mode mode{sync_mode::REALTIME};        ///< Sync mode
    conflict_strategy conflict_resolution{conflict_strategy::LAST_WRITE_WINS};
    std::chrono::seconds batch_interval{60};    ///< Batch interval (for BATCH mode)
    size_t batch_size{1000};                    ///< Max records per batch
    bool bidirectional{false};                  ///< Enable bidirectional replication
};

/**
 * @brief Replication observability configuration
 *
 * Configures logging and monitoring integration for the replication manager.
 */
struct replication_observability_config {
    /// Enable integrated logging
    bool enable_logging{true};

    /// Enable integrated monitoring
    bool enable_monitoring{true};

    /// Logger configuration
    integrated::db_logger_config logger_config;

    /// Monitoring configuration
    integrated::db_monitoring_config monitoring_config;
};

/**
 * @brief Replication event
 */
struct replication_event {
    enum class event_type {
        INSERT,
        UPDATE,
        DELETE
    } type;

    std::string table_name;
    std::unordered_map<std::string, std::string> old_values;
    std::unordered_map<std::string, std::string> new_values;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Replication statistics
 */
struct replication_stats {
    uint64_t events_replicated{0};
    uint64_t events_failed{0};
    uint64_t conflicts_resolved{0};
    std::chrono::milliseconds current_lag{0};
    std::chrono::milliseconds avg_lag{0};
    std::chrono::milliseconds max_lag{0};
    std::chrono::steady_clock::time_point last_event_time;
};

/**
 * @class replication_manager
 * @brief Manages database replication between nodes
 *
 * Features:
 * - Change Data Capture (CDC) for real-time replication
 * - Cross-database replication (PostgreSQL → MongoDB, etc.)
 * - Bidirectional replication support
 * - Conflict detection and resolution
 * - Replication lag monitoring
 * - Batch and real-time modes
 * - Table filtering and column selection
 *
 * Architecture:
 * - Source node: Captures changes using CDC or triggers
 * - Target node: Applies changes from replication queue
 * - Network transport: Uses network_system for reliable delivery
 * - Conflict resolution: Configurable strategy for handling conflicts
 *
 * Use Cases:
 * - Data migration between databases
 * - Real-time analytics (OLTP → OLAP)
 * - Geographic replication for disaster recovery
 * - Heterogeneous database sync (SQL → NoSQL)
 *
 * Example Usage:
 * @code
 *   auto replication = std::make_shared<replication_manager>();
 *
 *   // Configure source (PostgreSQL)
 *   distributed::node_config source;
 *   source.id = "postgres-source";
 *   source.host = "postgres.example.com";
 *   source.port = 5432;
 *   source.database = "production";
 *
 *   // Configure target (MongoDB)
 *   distributed::node_config target;
 *   target.id = "mongo-target";
 *   target.host = "mongodb.example.com";
 *   target.port = 27017;
 *   target.database = "analytics";
 *
 *   // Set up replication
 *   replication_config config;
 *   config.mode = sync_mode::REALTIME;
 *   config.conflict_resolution = conflict_strategy::LAST_WRITE_WINS;
 *   config.tables = {
 *       {"users", "users_collection"},
 *       {"orders", "orders_collection"}
 *   };
 *
 *   // Start replication
 *   auto result = replication->start_replication(source, target, config);
 *
 *   // Monitor lag
 *   auto lag = replication->get_replication_lag();
 *   if (lag > std::chrono::seconds(5)) {
 *       std::cerr << "Replication lag: " << lag.count() << "ms\n";
 *   }
 * @endcode
 */
class replication_manager {
public:
    /**
     * @brief Construct a new replication manager
     */
    replication_manager();

    /**
     * @brief Destructor - ensures cleanup
     */
    ~replication_manager();

    /**
     * @brief Start replication from source to target
     * @param source Source node configuration
     * @param target Target node configuration
     * @param config Replication configuration
     * @return result::ok() on success, error on failure
     */
    result<void> start_replication(const distributed::node_config& source,
                                    const distributed::node_config& target,
                                    const replication_config& config);

    /**
     * @brief Stop replication
     * @return result::ok() on success, error on failure
     */
    result<void> stop_replication();

    /**
     * @brief Check if replication is active
     * @return true if replication is running
     */
    bool is_active() const { return active_.load(); }

    /**
     * @brief Get current replication lag
     * @return Lag duration in milliseconds
     *
     * Replication lag is the time difference between when a change occurs
     * on the source and when it is applied to the target.
     */
    std::chrono::milliseconds get_replication_lag() const;

    /**
     * @brief Get replication statistics
     * @return Replication statistics
     */
    replication_stats get_stats() const;

    /**
     * @brief Set conflict resolution strategy
     * @param strategy Conflict resolution strategy
     */
    void set_conflict_resolution(conflict_strategy strategy);

    /**
     * @brief Manually trigger replication (for MANUAL mode)
     * @return result::ok() on success, error on failure
     */
    result<void> trigger_replication();

    /**
     * @brief Pause replication temporarily
     * @return result::ok() on success, error on failure
     */
    result<void> pause();

    /**
     * @brief Resume paused replication
     * @return result::ok() on success, error on failure
     */
    result<void> resume();

    /**
     * @brief Check replication health
     * @return true if replication is healthy (lag < threshold)
     */
    bool is_healthy() const;

    /**
     * @brief Get pending event count
     * @return Number of events waiting to be replicated
     */
    size_t get_pending_event_count() const;

    /**
     * @brief Configure observability (logging and monitoring)
     * @param config Observability configuration
     * @return result::ok() on success, error on failure
     */
    result<void> configure_observability(const replication_observability_config& config);

private:
    /**
     * @brief Initialize source connection and CDC
     * @return result::ok() on success, error on failure
     */
    result<void> initialize_source();

    /**
     * @brief Initialize target connection
     * @return result::ok() on success, error on failure
     */
    result<void> initialize_target();

    /**
     * @brief CDC worker thread function (captures changes)
     */
    void cdc_worker();

    /**
     * @brief Replication worker thread function (applies changes)
     */
    void replication_worker();

    /**
     * @brief Capture change event from source
     * @return Change event or empty optional if none available
     */
    std::optional<replication_event> capture_change_event();

    /**
     * @brief Apply change event to target
     * @param event Replication event
     * @return result::ok() on success, error on failure
     */
    result<void> apply_change_event(const replication_event& event);

    /**
     * @brief Resolve conflict between source and target
     * @param event Conflicting event
     * @return Resolved event
     */
    replication_event resolve_conflict(const replication_event& event);

    /**
     * @brief Update replication statistics
     * @param success Whether replication succeeded
     * @param lag Replication lag
     */
    void update_stats(bool success, std::chrono::milliseconds lag);

    // Node configurations
    distributed::node_config source_config_;
    distributed::node_config target_config_;
    replication_config config_;

    // Database clients
    std::shared_ptr<core::database_backend> source_client_;
    std::shared_ptr<core::database_backend> target_client_;

    // Replication state
    std::atomic<bool> active_{false};
    std::atomic<bool> paused_{false};

    // Worker threads
    std::thread cdc_thread_;
    std::thread replication_thread_;

    // Event queue
    mutable std::mutex queue_mutex_;
    std::vector<replication_event> event_queue_;

    // Statistics
    mutable std::mutex stats_mutex_;
    replication_stats stats_;

    // CDC strategy for capturing source changes
    std::unique_ptr<cdc::cdc_strategy_interface> cdc_strategy_;

    // Observability (logging and monitoring integration)
    replication_observability_config observability_config_;
    std::unique_ptr<integrated::adapters::logger_adapter> logger_;
    std::unique_ptr<integrated::adapters::monitoring_adapter> monitor_;
};

} // namespace database::replication
