/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include "../../core/result.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace database::replication {

// Forward declaration for CDC usage
struct replication_event;

} // namespace database::replication

namespace database::replication::cdc {

// Use the replication_event from parent namespace
using database::replication::replication_event;

/**
 * @brief Database type for CDC strategy selection
 */
enum class database_type {
    SQLITE,
    POSTGRESQL,
    MYSQL,
    MONGODB
};

/**
 * @brief CDC initialization configuration
 */
struct cdc_config {
    std::string connection_string;              ///< Database connection string
    std::vector<std::string> tracked_tables;    ///< Tables to track for changes
    bool capture_old_values{true};              ///< Capture old values for UPDATE/DELETE
    size_t max_batch_size{1000};                ///< Maximum events to fetch at once
    std::string change_table_prefix{"_cdc_"};   ///< Prefix for change tracking tables
};

/**
 * @interface cdc_strategy_interface
 * @brief Abstract interface for Change Data Capture implementations
 *
 * This interface defines the contract for database-specific CDC implementations.
 * Each database type (SQLite, PostgreSQL, MySQL, MongoDB) will have its own
 * implementation that captures changes using the most efficient method available.
 *
 * Implementation strategies:
 * - SQLite: Trigger-based change capture with shadow tables
 * - PostgreSQL: Logical replication slots with pgoutput plugin
 * - MySQL: Binary log (binlog) parsing
 * - MongoDB: Change streams
 */
class cdc_strategy_interface {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~cdc_strategy_interface() = default;

    /**
     * @brief Initialize CDC for the source database
     * @param config CDC configuration
     * @return result::ok() on success, error on failure
     *
     * This method should:
     * - Connect to the database
     * - Set up change tracking infrastructure (triggers, replication slots, etc.)
     * - Prepare for capturing changes
     */
    virtual result<void> initialize(const cdc_config& config) = 0;

    /**
     * @brief Start capturing changes
     * @return result::ok() on success, error on failure
     *
     * Begin actively capturing database changes.
     */
    virtual result<void> start() = 0;

    /**
     * @brief Stop capturing changes
     * @return result::ok() on success, error on failure
     *
     * Stop capturing changes but maintain infrastructure.
     */
    virtual result<void> stop() = 0;

    /**
     * @brief Capture the next available change event
     * @return Change event or empty optional if none available
     *
     * This is a non-blocking call that returns immediately if no events are available.
     */
    virtual std::optional<replication_event> capture_next_event() = 0;

    /**
     * @brief Capture multiple change events in a batch
     * @param max_count Maximum number of events to capture
     * @return Vector of captured events (may be empty)
     */
    virtual std::vector<replication_event> capture_events(size_t max_count) = 0;

    /**
     * @brief Acknowledge that an event has been processed
     * @param event The event that was processed
     * @return result::ok() on success, error on failure
     *
     * This allows the CDC system to clean up processed events and maintain position.
     */
    virtual result<void> acknowledge_event(const replication_event& event) = 0;

    /**
     * @brief Get the current position in the change stream
     * @return Position string (format depends on implementation)
     *
     * This can be used for resuming from a known position.
     */
    virtual std::string get_current_position() const = 0;

    /**
     * @brief Set the position in the change stream
     * @param position Position string
     * @return result::ok() on success, error on failure
     *
     * This allows resuming from a previously saved position.
     */
    virtual result<void> set_position(const std::string& position) = 0;

    /**
     * @brief Check if CDC is currently active
     * @return true if actively capturing changes
     */
    virtual bool is_active() const = 0;

    /**
     * @brief Get the database type this strategy supports
     * @return Database type enum
     */
    virtual database_type get_database_type() const = 0;

    /**
     * @brief Clean up CDC infrastructure
     * @return result::ok() on success, error on failure
     *
     * Remove change tracking tables, triggers, replication slots, etc.
     * Use with caution - this is destructive.
     */
    virtual result<void> cleanup() = 0;

    /**
     * @brief Get pending event count
     * @return Number of unprocessed events
     */
    virtual size_t get_pending_count() const = 0;
};

} // namespace database::replication::cdc
