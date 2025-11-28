/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include "cdc_strategy_interface.h"
#include "../replication_manager.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <queue>

// Forward declaration for MySQL handle
struct MYSQL;

namespace database::replication::cdc {

/**
 * @class mysql_cdc_strategy
 * @brief MySQL-specific CDC implementation using Binary Log (binlog)
 *
 * This implementation uses MySQL's binary log to capture changes.
 * It supports:
 *
 * 1. ROW-based binary logging (required for CDC)
 * 2. GTID-based positioning for reliable resume
 * 3. Table filtering
 *
 * Requirements:
 * - MySQL 5.6+ (for GTID support) or MySQL 5.5+ (for basic binlog)
 * - binlog_format = 'ROW' in my.cnf
 * - binlog_row_image = 'FULL' (recommended)
 * - User must have REPLICATION SLAVE privilege
 *
 * CDC Flow:
 * 1. Register as a replication slave
 * 2. Request binlog events from specified position
 * 3. Parse ROW events for INSERT/UPDATE/DELETE
 * 4. Convert to replication_event format
 * 5. Track position via binlog file/position or GTID
 *
 * Advantages:
 * - Uses existing MySQL replication infrastructure
 * - Low overhead (binlog is already written)
 * - Captures all row changes
 * - Supports GTID for position tracking
 *
 * Limitations:
 * - Requires ROW binlog format
 * - DDL changes need additional handling
 * - Binary log must be retained long enough
 *
 * Example:
 * @code
 *   mysql_cdc_strategy cdc;
 *   cdc_config config;
 *   config.connection_string = "mysql://user:pass@localhost:3306/dbname";
 *   config.tracked_tables = {"users", "orders"};
 *
 *   auto result = cdc.initialize(config);
 *   if (result.is_ok()) {
 *       cdc.start();
 *
 *       while (auto event = cdc.capture_next_event()) {
 *           process_event(*event);
 *           cdc.acknowledge_event(*event);
 *       }
 *   }
 * @endcode
 */
class mysql_cdc_strategy : public cdc_strategy_interface {
public:
    /**
     * @brief Construct a new MySQL CDC strategy
     */
    mysql_cdc_strategy();

    /**
     * @brief Destructor - cleans up resources
     */
    ~mysql_cdc_strategy() override;

    // Non-copyable
    mysql_cdc_strategy(const mysql_cdc_strategy&) = delete;
    mysql_cdc_strategy& operator=(const mysql_cdc_strategy&) = delete;

    // Movable
    mysql_cdc_strategy(mysql_cdc_strategy&&) noexcept;
    mysql_cdc_strategy& operator=(mysql_cdc_strategy&&) noexcept;

    /**
     * @brief Initialize CDC for MySQL database
     * @param config CDC configuration
     * @return result::ok() on success, error on failure
     */
    result<void> initialize(const cdc_config& config) override;

    /**
     * @brief Start capturing changes
     * @return result::ok() on success, error on failure
     */
    result<void> start() override;

    /**
     * @brief Stop capturing changes
     * @return result::ok() on success, error on failure
     */
    result<void> stop() override;

    /**
     * @brief Capture the next available change event
     * @return Change event or empty optional if none available
     */
    std::optional<replication_event> capture_next_event() override;

    /**
     * @brief Capture multiple change events in a batch
     * @param max_count Maximum number of events to capture
     * @return Vector of captured events
     */
    std::vector<replication_event> capture_events(size_t max_count) override;

    /**
     * @brief Acknowledge that an event has been processed
     * @param event The event that was processed
     * @return result::ok() on success, error on failure
     */
    result<void> acknowledge_event(const replication_event& event) override;

    /**
     * @brief Get the current binlog position
     * @return Position as "binlog_file:position" or GTID
     */
    std::string get_current_position() const override;

    /**
     * @brief Set the binlog position to resume from
     * @param position Position string
     * @return result::ok() on success, error on failure
     */
    result<void> set_position(const std::string& position) override;

    /**
     * @brief Check if CDC is active
     * @return true if actively capturing
     */
    bool is_active() const override;

    /**
     * @brief Get the database type
     * @return database_type::MYSQL
     */
    database_type get_database_type() const override;

    /**
     * @brief Clean up all CDC infrastructure
     * @return result::ok() on success, error on failure
     */
    result<void> cleanup() override;

    /**
     * @brief Get pending event count
     * @return Number of unprocessed events
     */
    size_t get_pending_count() const override;

private:
    /**
     * @brief Parse connection string to get connection parameters
     */
    void parse_connection_string();

    /**
     * @brief Get current binlog file and position from server
     * @return result::ok() on success, error on failure
     */
    result<void> fetch_current_binlog_position();

    /**
     * @brief Binlog streaming worker thread
     */
    void binlog_worker();

    /**
     * @brief Parse binlog row event
     * @param event_type Event type (WRITE, UPDATE, DELETE)
     * @param table_name Table name
     * @param row_data Row data
     * @return Parsed replication event
     */
    replication_event parse_row_event(
        int event_type,
        const std::string& table_name,
        const std::string& row_data);

    /**
     * @brief Execute a SQL query
     * @param sql SQL query
     * @return result::ok() on success, error on failure
     */
    result<void> execute_sql(const std::string& sql);

    /**
     * @brief Get last error message from connection
     * @return Error message
     */
    std::string get_last_error() const;

    // MySQL connection
    MYSQL* conn_{nullptr};

    // Configuration
    cdc_config config_;
    std::string host_;
    int port_{3306};
    std::string user_;
    std::string password_;
    std::string database_;

    // Binlog position
    std::string binlog_file_;
    uint64_t binlog_position_{0};
    std::string gtid_set_;
    bool use_gtid_{false};

    // Server ID for replication
    uint32_t server_id_{0};

    // State
    std::atomic<bool> active_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> stop_requested_{false};

    // Binlog worker
    std::thread binlog_thread_;

    // Event queue
    mutable std::mutex queue_mutex_;
    std::queue<replication_event> event_queue_;

    // Thread safety
    mutable std::mutex mutex_;

    // Tracked tables
    std::unordered_set<std::string> tracked_tables_;

    // Table map cache (table_id -> table_name)
    std::unordered_map<uint64_t, std::string> table_map_;
};

} // namespace database::replication::cdc
