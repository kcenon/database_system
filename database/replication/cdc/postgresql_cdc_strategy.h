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

// Forward declaration for PostgreSQL handle
struct pg_conn;
typedef pg_conn PGconn;

namespace database::replication::cdc {

/**
 * @class postgresql_cdc_strategy
 * @brief PostgreSQL-specific CDC implementation using Logical Replication
 *
 * This implementation uses PostgreSQL's logical replication feature to capture
 * changes in the database. It creates:
 *
 * 1. A replication slot for streaming WAL changes
 * 2. A publication for the tracked tables
 * 3. Decodes WAL using the 'pgoutput' plugin
 *
 * Requirements:
 * - PostgreSQL 10+ (for pgoutput plugin)
 * - wal_level = 'logical' in postgresql.conf
 * - User must have REPLICATION privilege
 *
 * CDC Flow:
 * 1. Create replication slot and publication
 * 2. Start streaming from the slot
 * 3. Decode WAL changes using pgoutput
 * 4. Convert to replication_event format
 * 5. Acknowledge consumed LSN
 *
 * Advantages:
 * - Low overhead (uses WAL which is already written)
 * - No trigger overhead
 * - Captures all DML changes atomically
 * - Supports transaction boundaries
 *
 * Limitations:
 * - Requires PostgreSQL 10+
 * - Requires superuser or replication privilege
 * - DDL changes are not captured
 *
 * Example:
 * @code
 *   postgresql_cdc_strategy cdc;
 *   cdc_config config;
 *   config.connection_string = "postgresql://user:pass@localhost:5432/dbname";
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
class postgresql_cdc_strategy : public cdc_strategy_interface {
public:
    /**
     * @brief Construct a new PostgreSQL CDC strategy
     */
    postgresql_cdc_strategy();

    /**
     * @brief Destructor - cleans up resources
     */
    ~postgresql_cdc_strategy() override;

    // Non-copyable
    postgresql_cdc_strategy(const postgresql_cdc_strategy&) = delete;
    postgresql_cdc_strategy& operator=(const postgresql_cdc_strategy&) = delete;

    // Movable
    postgresql_cdc_strategy(postgresql_cdc_strategy&&) noexcept;
    postgresql_cdc_strategy& operator=(postgresql_cdc_strategy&&) noexcept;

    /**
     * @brief Initialize CDC for PostgreSQL database
     * @param config CDC configuration
     * @return result::ok() on success, error on failure
     *
     * Creates replication slot and publication for tracked tables.
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
     * @brief Get the current LSN position
     * @return Position as string representation of LSN
     */
    std::string get_current_position() const override;

    /**
     * @brief Set the LSN position to resume from
     * @param position Position string (LSN)
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
     * @return database_type::POSTGRESQL
     */
    database_type get_database_type() const override;

    /**
     * @brief Clean up all CDC infrastructure
     * @return result::ok() on success, error on failure
     *
     * Drops replication slot and publication.
     */
    result<void> cleanup() override;

    /**
     * @brief Get pending event count
     * @return Number of unprocessed events
     */
    size_t get_pending_count() const override;

private:
    /**
     * @brief Create replication slot
     * @return result::ok() on success, error on failure
     */
    result<void> create_replication_slot();

    /**
     * @brief Create publication for tracked tables
     * @return result::ok() on success, error on failure
     */
    result<void> create_publication();

    /**
     * @brief Start replication streaming thread
     */
    void streaming_worker();

    /**
     * @brief Parse pgoutput message to replication event
     * @param data Raw message data
     * @param len Message length
     * @return Parsed event or nullopt if not a data message
     */
    std::optional<replication_event> parse_pgoutput_message(
        const char* data, size_t len);

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

    // PostgreSQL connections
    PGconn* conn_{nullptr};            ///< Main connection
    PGconn* repl_conn_{nullptr};       ///< Replication connection

    // Configuration
    cdc_config config_;
    std::string slot_name_{"cdc_slot"};
    std::string publication_name_{"cdc_publication"};

    // State
    std::atomic<bool> active_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> stop_requested_{false};
    std::string current_lsn_;
    std::string confirmed_lsn_;

    // Streaming worker
    std::thread streaming_thread_;

    // Event queue
    mutable std::mutex queue_mutex_;
    std::queue<replication_event> event_queue_;

    // Thread safety
    mutable std::mutex mutex_;

    // Tracked tables
    std::unordered_set<std::string> tracked_tables_;
};

} // namespace database::replication::cdc
