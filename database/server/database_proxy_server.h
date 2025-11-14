/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "database_session.h"
#include "../connection_pool.h"
#include "../protocol/database_protocol.h"

namespace database::server {

/**
 * @class database_proxy_server
 * @brief Database proxy server that provides remote access to databases
 *
 * Features:
 * - TCP server for remote database access
 * - Session management for multiple clients
 * - Query routing to connection pool
 * - Transaction support
 * - TLS/SSL encryption (via network_system)
 *
 * Architecture:
 * - Uses network_system::messaging_server (TODO: integrate)
 * - Connection pool for database connections
 * - Thread pool for query execution
 *
 * @note This is a stub implementation. Full network integration is TODO.
 */
class database_proxy_server {
public:
    /**
     * @brief Construct a new database proxy server
     * @param port Server port
     * @param db_pool Connection pool for database connections
     */
    database_proxy_server(uint16_t port, std::shared_ptr<connection_pool_manager> db_pool);

    /**
     * @brief Destructor - ensures cleanup
     */
    ~database_proxy_server();

    /**
     * @brief Start the server
     * @return true if started successfully, false otherwise
     *
     * TODO: Integrate network_system::messaging_server
     */
    bool start();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     * @return true if running, false otherwise
     */
    [[nodiscard]] bool is_running() const { return running_.load(); }

    /**
     * @brief Get active session count
     * @return Number of active sessions
     */
    [[nodiscard]] size_t get_active_session_count() const;

private:
    /**
     * @brief Handle client connection
     * @param session_id New session ID
     *
     * TODO: Implement with network_system
     */
    void handle_client_connect(const std::string& session_id);

    /**
     * @brief Handle client disconnection
     * @param session_id Session ID
     *
     * TODO: Implement with network_system
     */
    void handle_client_disconnect(const std::string& session_id);

    /**
     * @brief Handle incoming message
     * @param session_id Session ID
     * @param data Message data
     *
     * TODO: Implement message processing
     */
    void handle_message(const std::string& session_id, const std::vector<uint8_t>& data);

    uint16_t port_;
    std::shared_ptr<connection_pool_manager> db_pool_;

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<database_session>> sessions_;

    std::atomic<bool> running_;
    std::atomic<uint64_t> next_session_id_;
};

} // namespace database::server
