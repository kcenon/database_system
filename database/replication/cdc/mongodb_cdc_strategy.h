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

namespace database::replication::cdc {

/**
 * @class mongodb_cdc_strategy
 * @brief MongoDB-specific CDC implementation using Change Streams
 *
 * This implementation uses MongoDB's Change Streams feature to capture
 * real-time changes to documents. It supports:
 *
 * 1. Collection-level change streams
 * 2. Database-level change streams
 * 3. Resume token for reliable position tracking
 * 4. Full document lookup for updates
 *
 * Requirements:
 * - MongoDB 3.6+ (for change streams)
 * - Replica set or sharded cluster (required for change streams)
 * - readConcern: "majority" availability
 *
 * CDC Flow:
 * 1. Open change stream on specified collections/database
 * 2. Receive change events in real-time
 * 3. Convert to replication_event format
 * 4. Store resume token for position tracking
 *
 * Change Event Types:
 * - insert: New document inserted
 * - update: Document updated (includes updateDescription)
 * - replace: Document replaced entirely
 * - delete: Document deleted
 * - invalidate: Collection dropped or renamed
 *
 * Advantages:
 * - Real-time event delivery
 * - Guaranteed ordering
 * - Resumable from any position
 * - No polling overhead
 *
 * Limitations:
 * - Requires replica set or sharded cluster
 * - Cannot capture changes on standalone instances
 * - Oplog must be retained long enough
 *
 * Example:
 * @code
 *   mongodb_cdc_strategy cdc;
 *   cdc_config config;
 *   config.connection_string = "mongodb://localhost:27017/mydb";
 *   config.tracked_tables = {"users", "orders"};  // Collection names
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
class mongodb_cdc_strategy : public cdc_strategy_interface {
public:
    /**
     * @brief Construct a new MongoDB CDC strategy
     */
    mongodb_cdc_strategy();

    /**
     * @brief Destructor - cleans up resources
     */
    ~mongodb_cdc_strategy() override;

    // Non-copyable
    mongodb_cdc_strategy(const mongodb_cdc_strategy&) = delete;
    mongodb_cdc_strategy& operator=(const mongodb_cdc_strategy&) = delete;

    // Movable
    mongodb_cdc_strategy(mongodb_cdc_strategy&&) noexcept;
    mongodb_cdc_strategy& operator=(mongodb_cdc_strategy&&) noexcept;

    /**
     * @brief Initialize CDC for MongoDB database
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
     * @brief Get the current resume token
     * @return Resume token as string
     */
    std::string get_current_position() const override;

    /**
     * @brief Set the resume token to resume from
     * @param position Resume token string
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
     * @return database_type::MONGODB
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
     * @brief Parse connection string to extract database name
     */
    void parse_connection_string();

    /**
     * @brief Change stream worker thread
     */
    void change_stream_worker();

    /**
     * @brief Parse MongoDB change event to replication_event
     * @param operation_type Operation type (insert, update, delete, replace)
     * @param collection Collection name
     * @param document_key Document key (typically _id)
     * @param full_document Full document content (for insert/replace/update with fullDocument)
     * @param update_description Update description (for update operations)
     * @return Parsed replication event
     */
    replication_event parse_change_event(
        const std::string& operation_type,
        const std::string& collection,
        const std::string& document_key,
        const std::string& full_document,
        const std::string& update_description);

    // Configuration
    cdc_config config_;
    std::string uri_;
    std::string database_name_;

    // Resume token for position tracking
    std::string resume_token_;

    // State
    std::atomic<bool> active_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> stop_requested_{false};

    // Change stream worker
    std::thread change_stream_thread_;

    // Event queue
    mutable std::mutex queue_mutex_;
    std::queue<replication_event> event_queue_;

    // Thread safety
    mutable std::mutex mutex_;

    // Tracked collections
    std::unordered_set<std::string> tracked_collections_;

    // MongoDB client handle (opaque pointer for forward compatibility)
    void* client_{nullptr};
};

} // namespace database::replication::cdc
