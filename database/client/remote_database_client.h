/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <memory>
#include <future>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "../core/database_backend.h"
#include "../protocol/database_protocol.h"

namespace database::client {

/**
 * @class remote_database_client
 * @brief Remote database client that connects to database_proxy_server
 *
 * Implements database_backend interface for transparent remote database access.
 * Clients can use this exactly like a local database backend.
 *
 * Features:
 * - database_backend interface compatibility
 * - Auto-reconnect with exponential backoff
 * - Async query support
 * - TLS/SSL encryption (via network_system)
 * - Request-response correlation
 *
 * Architecture:
 * - Uses network_system::resilient_client (TODO: integrate)
 * - Binary protocol for communication
 * - Thread-safe request/response handling
 *
 * Example Usage:
 * @code
 *   auto client = std::make_unique<remote_database_client>();
 *   core::connection_config config;
 *   config.host = "localhost";
 *   config.port = 5432;
 *
 *   if (auto result = client->initialize(config); result.has_error()) {
 *       // Handle error
 *   }
 *
 *   auto rows = client->select_query("SELECT * FROM users");
 *   if (rows.is_ok()) {
 *       for (const auto& row : rows.value()) {
 *           // Process row
 *       }
 *   }
 * @endcode
 *
 * @note This is a stub implementation. Full network integration is TODO.
 */
class remote_database_client : public core::database_backend {
public:
    /**
     * @brief Construct a new remote database client
     */
    remote_database_client();

    /**
     * @brief Destructor - ensures cleanup
     */
    ~remote_database_client() override;

    // database_backend interface implementation

    /**
     * @brief Get the database type
     * @return Database type (REMOTE)
     */
    database_types type() const override;

    /**
     * @brief Initialize connection to remote database server
     * @param config Connection configuration (host, port, credentials)
     * @return VoidResult::ok() on success, error on failure
     *
     * TODO: Integrate network_system::resilient_client
     */
    database::VoidResult initialize(const core::connection_config& config) override;

    /**
     * @brief Shutdown connection gracefully
     * @return VoidResult::ok() on success, error on failure
     */
    database::VoidResult shutdown() override;

    /**
     * @brief Check if client is connected and ready
     * @return true if connected, false otherwise
     */
    [[nodiscard]] bool is_initialized() const override;

    /**
     * @brief Execute INSERT query remotely
     * @param query_string INSERT statement
     * @return Number of rows inserted, or error
     */
    database::Result<uint64_t> insert_query(const std::string& query_string) override;

    /**
     * @brief Execute UPDATE query remotely
     * @param query_string UPDATE statement
     * @return Number of rows updated, or error
     */
    database::Result<uint64_t> update_query(const std::string& query_string) override;

    /**
     * @brief Execute DELETE query remotely
     * @param query_string DELETE statement
     * @return Number of rows deleted, or error
     */
    database::Result<uint64_t> delete_query(const std::string& query_string) override;

    /**
     * @brief Execute SELECT query remotely
     * @param query_string SELECT statement
     * @return Query results, or error
     */
    database::Result<core::database_result> select_query(const std::string& query_string) override;

    /**
     * @brief Execute general query remotely
     * @param query_string SQL statement
     * @return VoidResult::ok() on success, error on failure
     */
    database::VoidResult execute_query(const std::string& query_string) override;

    /**
     * @brief Begin remote transaction
     * @return VoidResult::ok() on success, error on failure
     */
    database::VoidResult begin_transaction() override;

    /**
     * @brief Commit remote transaction
     * @return VoidResult::ok() on success, error on failure
     */
    database::VoidResult commit_transaction() override;

    /**
     * @brief Rollback remote transaction
     * @return VoidResult::ok() on success, error on failure
     */
    database::VoidResult rollback_transaction() override;

    /**
     * @brief Check if in remote transaction
     * @return true if transaction active, false otherwise
     */
    [[nodiscard]] bool in_transaction() const override;

    /**
     * @brief Get last error message
     * @return Error message string
     */
    [[nodiscard]] std::string last_error() const override;

    /**
     * @brief Get connection information
     * @return Map of connection properties
     */
    [[nodiscard]] std::map<std::string, std::string> connection_info() const override;

    // Remote-specific async API

    /**
     * @brief Execute query asynchronously
     * @param query_string SQL statement
     * @return Future with query result
     *
     * Allows non-blocking query execution with futures.
     */
    std::future<database::Result<core::database_result>> execute_async(const std::string& query_string);

private:
    /**
     * @brief Send request and wait for response
     * @param request_type Message type
     * @param payload Request payload
     * @return Response payload, or error
     *
     * TODO: Implement with network_system
     */
    database::Result<std::vector<uint8_t>> send_request(
        protocol::message_type request_type,
        const std::vector<uint8_t>& payload
    );

    /**
     * @brief Handle incoming response message
     * @param message_data Raw message bytes
     *
     * TODO: Implement message routing to pending requests
     */
    void handle_response(const std::vector<uint8_t>& message_data);

    /**
     * @brief Generate unique request ID
     * @return Request ID
     */
    uint64_t next_request_id();

    // TODO: Network layer (requires network_system)
    // std::shared_ptr<network_system::resilient_client> network_client_;

    // Request/response tracking
    mutable std::mutex requests_mutex_;
    std::unordered_map<uint64_t, std::promise<protocol::query_response>> pending_requests_;
    std::atomic<uint64_t> next_request_id_{1};

    // Connection state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> in_transaction_{false};
    core::connection_config config_;
    mutable std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace database::client
