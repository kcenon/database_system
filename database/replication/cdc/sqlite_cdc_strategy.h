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
#include <unordered_set>

// Forward declaration for SQLite handle
struct sqlite3;

namespace database::replication::cdc {

/**
 * @class sqlite_cdc_strategy
 * @brief SQLite-specific CDC implementation using triggers
 *
 * This implementation uses triggers and shadow tables to capture changes
 * in SQLite databases. For each tracked table, it creates:
 *
 * 1. A change log table (_cdc_<table>_changes) that stores:
 *    - Operation type (INSERT, UPDATE, DELETE)
 *    - Timestamp
 *    - Old values (for UPDATE/DELETE)
 *    - New values (for INSERT/UPDATE)
 *    - Processed flag
 *
 * 2. Triggers for INSERT, UPDATE, and DELETE operations
 *
 * The change log is polled periodically to capture events.
 *
 * Advantages:
 * - Works with any SQLite database
 * - No external dependencies
 * - Captures all DML changes
 *
 * Limitations:
 * - Adds overhead to write operations
 * - Change log tables grow and need periodic cleanup
 * - Schema changes require trigger recreation
 *
 * Example:
 * @code
 *   sqlite_cdc_strategy cdc;
 *   cdc_config config;
 *   config.connection_string = "path/to/database.db";
 *   config.tracked_tables = {"users", "orders"};
 *
 *   auto result = cdc.initialize(config);
 *   if (result.is_ok()) {
 *       cdc.start();
 *
 *       while (auto event = cdc.capture_next_event()) {
 *           // Process event
 *           process_event(*event);
 *           cdc.acknowledge_event(*event);
 *       }
 *   }
 * @endcode
 */
class sqlite_cdc_strategy : public cdc_strategy_interface {
public:
    /**
     * @brief Construct a new sqlite cdc strategy
     */
    sqlite_cdc_strategy();

    /**
     * @brief Destructor - cleans up resources
     */
    ~sqlite_cdc_strategy() override;

    // Non-copyable
    sqlite_cdc_strategy(const sqlite_cdc_strategy&) = delete;
    sqlite_cdc_strategy& operator=(const sqlite_cdc_strategy&) = delete;

    // Movable
    sqlite_cdc_strategy(sqlite_cdc_strategy&&) noexcept;
    sqlite_cdc_strategy& operator=(sqlite_cdc_strategy&&) noexcept;

    /**
     * @brief Initialize CDC for SQLite database
     * @param config CDC configuration
     * @return result::ok() on success, error on failure
     *
     * Creates change log tables and triggers for tracked tables.
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
     * @brief Get the current position (last processed change ID)
     * @return Position as string representation of change ID
     */
    std::string get_current_position() const override;

    /**
     * @brief Set the position to resume from
     * @param position Position string (change ID)
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
     * @return database_type::SQLITE
     */
    database_type get_database_type() const override;

    /**
     * @brief Clean up all CDC infrastructure
     * @return result::ok() on success, error on failure
     *
     * Drops all change log tables and triggers.
     */
    result<void> cleanup() override;

    /**
     * @brief Get pending event count
     * @return Number of unprocessed events
     */
    size_t get_pending_count() const override;

private:
    /**
     * @brief Create change log table for a tracked table
     * @param table_name Name of the table to track
     * @return result::ok() on success, error on failure
     */
    result<void> create_change_table(const std::string& table_name);

    /**
     * @brief Create triggers for a tracked table
     * @param table_name Name of the table to track
     * @return result::ok() on success, error on failure
     */
    result<void> create_triggers(const std::string& table_name);

    /**
     * @brief Get column information for a table
     * @param table_name Name of the table
     * @return Vector of column names
     */
    std::vector<std::string> get_table_columns(const std::string& table_name);

    /**
     * @brief Parse a change record into a replication event
     * @param table_name Table name
     * @param change_id Change ID
     * @param operation Operation type (INSERT, UPDATE, DELETE)
     * @param old_values JSON string of old values
     * @param new_values JSON string of new values
     * @param timestamp Timestamp string
     * @return Parsed replication event
     */
    replication_event parse_change_record(
        const std::string& table_name,
        int64_t change_id,
        const std::string& operation,
        const std::string& old_values,
        const std::string& new_values,
        const std::string& timestamp
    );

    /**
     * @brief Parse JSON string to key-value map
     * @param json_str JSON string
     * @return Parsed map
     */
    std::unordered_map<std::string, std::string> parse_json_values(
        const std::string& json_str
    );

    /**
     * @brief Execute a SQL statement
     * @param sql SQL statement
     * @return result::ok() on success, error on failure
     */
    result<void> execute_sql(const std::string& sql);

    // SQLite connection
    sqlite3* db_{nullptr};

    // Configuration
    cdc_config config_;

    // State
    std::atomic<bool> active_{false};
    std::atomic<bool> initialized_{false};
    int64_t last_processed_id_{0};

    // Thread safety
    mutable std::mutex mutex_;

    // Tracked tables
    std::unordered_set<std::string> tracked_tables_;
};

} // namespace database::replication::cdc
