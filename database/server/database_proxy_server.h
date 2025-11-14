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

// network_system integration
#ifdef BUILD_WITH_COMMON_SYSTEM
#include <network_system/core/messaging_server.h>
#include <network_system/session/messaging_session.h>
#endif

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
 * - Uses network_system::messaging_server for network communication
 * - Connection pool for database connections
 * - Thread pool for query execution
 * - Binary protocol for efficient communication
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
     * Uses network_system::messaging_server for network communication
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
     * @param network_session network_system messaging session
     */
    void handle_client_connect(
#ifdef BUILD_WITH_COMMON_SYSTEM
        std::shared_ptr<network_system::session::messaging_session> network_session
#else
        const std::string& session_id
#endif
    );

    /**
     * @brief Handle client disconnection
     * @param session_id Session ID
     */
    void handle_client_disconnect(const std::string& session_id);

    /**
     * @brief Handle incoming message
     * @param network_session Network session
     * @param data Message data
     */
    void handle_message(
#ifdef BUILD_WITH_COMMON_SYSTEM
        std::shared_ptr<network_system::session::messaging_session> network_session,
#else
        const std::string& session_id,
#endif
        const std::vector<uint8_t>& data
    );

    /**
     * @brief Process query request message
     * @param header Message header
     * @param payload Request payload
     * @return Response payload
     */
    std::vector<uint8_t> process_query_request(
        const protocol::message_header& header,
        const std::vector<uint8_t>& payload
    );

    uint16_t port_;
    std::shared_ptr<connection_pool_manager> db_pool_;

#ifdef BUILD_WITH_COMMON_SYSTEM
    std::shared_ptr<network_system::core::messaging_server> network_server_;
    std::unordered_map<std::string, std::shared_ptr<network_system::session::messaging_session>> network_sessions_;
#endif

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<database_session>> sessions_;

    std::atomic<bool> running_;
    std::atomic<uint64_t> next_session_id_;
};

} // namespace database::server
