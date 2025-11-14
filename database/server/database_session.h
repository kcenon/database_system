/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include "../database_base.h"
#include "../protocol/database_protocol.h"

namespace database::server {

/**
 * @class database_session
 * @brief Represents a client session on the database proxy server
 *
 * Each connected client has a session that manages:
 * - Connection state
 * - Transaction state
 * - Query execution
 * - Resource cleanup
 *
 * Thread-safe for concurrent operations.
 */
class database_session {
public:
    /**
     * @brief Construct a new database session
     * @param session_id Unique session identifier
     * @param db_connection Database connection for this session
     */
    database_session(std::string session_id, std::shared_ptr<database_base> db_connection);

    /**
     * @brief Destructor - ensures cleanup
     */
    ~database_session();

    /**
     * @brief Get session ID
     * @return Session ID
     */
    [[nodiscard]] const std::string& get_session_id() const { return session_id_; }

    /**
     * @brief Check if session is active
     * @return true if active, false otherwise
     */
    [[nodiscard]] bool is_active() const { return active_.load(); }

    /**
     * @brief Execute a query request
     * @param request Query request
     * @return Query response
     *
     * Supports SELECT, INSERT, UPDATE, DELETE, and DDL operations
     */
    protocol::query_response execute_query(const protocol::query_request& request);

    /**
     * @brief Begin a transaction
     * @return Transaction response
     *
     * Thread-safe transaction state management
     */
    protocol::transaction_response begin_transaction();

    /**
     * @brief Commit current transaction
     * @return Transaction response
     */
    protocol::transaction_response commit_transaction();

    /**
     * @brief Rollback current transaction
     * @return Transaction response
     */
    protocol::transaction_response rollback_transaction();

    /**
     * @brief Close the session
     */
    void close();

private:
    std::string session_id_;
    std::shared_ptr<database_base> db_connection_;
    std::atomic<bool> active_;
    std::atomic<bool> in_transaction_;
};

} // namespace database::server
