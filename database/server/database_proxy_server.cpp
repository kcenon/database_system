/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "database_proxy_server.h"
#include <iostream>

namespace database::server {

database_proxy_server::database_proxy_server(uint16_t port, std::shared_ptr<connection_pool_manager> db_pool)
    : port_(port)
    , db_pool_(std::move(db_pool))
    , running_(false)
    , next_session_id_(1)
{
}

database_proxy_server::~database_proxy_server() {
    stop();
}

bool database_proxy_server::start() {
    if (running_.exchange(true)) {
        return false;  // Already running
    }

    // TODO: Initialize network_system::messaging_server
    // TODO: Start accepting connections
    // TODO: Set up message handlers

    std::cout << "Database proxy server started on port " << port_ << " (stub implementation)\n";

    return true;
}

void database_proxy_server::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }

    // Close all sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->close();
    }
    sessions_.clear();

    // TODO: Stop network_system::messaging_server

    std::cout << "Database proxy server stopped\n";
}

size_t database_proxy_server::get_active_session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

void database_proxy_server::handle_client_connect(const std::string& session_id) {
    // TODO: Acquire connection from pool
    // TODO: Create session
    // std::lock_guard<std::mutex> lock(sessions_mutex_);
    // sessions_[session_id] = std::make_shared<database_session>(session_id, connection);
}

void database_proxy_server::handle_client_disconnect(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->close();
        sessions_.erase(it);
    }
}

void database_proxy_server::handle_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    // TODO: Deserialize message header
    // TODO: Route to appropriate handler based on message type
    // TODO: Serialize and send response
}

} // namespace database::server
