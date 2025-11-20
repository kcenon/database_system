/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "database_proxy_server.h"

#include <iostream>

namespace database::server {

database_proxy_server::database_proxy_server(
    uint16_t port,
    std::shared_ptr<connection_pool_manager> db_pool)
    : port_(port)
    , db_pool_(std::move(db_pool))
    , running_(false)
    , next_session_id_(1)
{
    network_server_ = std::make_shared<network_system::core::messaging_server>("database_proxy_server");
}

database_proxy_server::~database_proxy_server() {
    stop();
}

bool database_proxy_server::start() {
    if (running_.exchange(true)) {
        return false;
    }

    network_server_->set_connection_callback(
        [this](std::shared_ptr<network_system::session::messaging_session> session) {
            handle_client_connect(std::move(session));
        }
    );

    network_server_->set_disconnection_callback(
        [this](const std::string& session_id) {
            handle_client_disconnect(session_id);
        }
    );

    network_server_->set_receive_callback(
        [this](std::shared_ptr<network_system::session::messaging_session> session, const auto& data) {
            handle_message(std::move(session), data);
        }
    );

    network_server_->set_error_callback(
        [](auto session, std::error_code ec) {
            std::cerr << "Session error: " << ec.message() << "\n";
        }
    );

    auto result = network_server_->start_server(port_);
    if (result.is_err()) {
        running_ = false;
        std::cerr << "Failed to start network server: " << result.error().message << "\n";
        return false;
    }

    std::cout << "Database proxy server started on port " << port_ << "\n";
    return true;
}

void database_proxy_server::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : network_sessions_) {
            session->stop_session();
        }
        network_sessions_.clear();
    }

    if (network_server_) {
        auto result = network_server_->stop_server();
        if (result.is_err()) {
            std::cerr << "Failed to stop network server: " << result.error().message << "\n";
        }
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : sessions_) {
            session->close();
        }
        sessions_.clear();
    }

    std::cout << "Database proxy server stopped\n";
}

size_t database_proxy_server::get_active_session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

void database_proxy_server::handle_client_connect(
    std::shared_ptr<network_system::session::messaging_session> network_session
) {
    std::string session_id = "session_" + std::to_string(next_session_id_.fetch_add(1));

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        network_sessions_[session_id] = network_session;
    }

    std::cout << "Client connected: " << session_id << "\n";
    network_session->start_session();
}

void database_proxy_server::handle_client_disconnect(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    network_sessions_.erase(session_id);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->close();
        sessions_.erase(it);
    }

    std::cout << "Client disconnected: " << session_id << "\n";
}

void database_proxy_server::handle_message(
    std::shared_ptr<network_system::session::messaging_session> network_session,
    const std::vector<uint8_t>& data
) {
    auto header_result = protocol::protocol_serializer::deserialize_header(data);
    if (!header_result.has_value()) {
        std::cerr << "Failed to deserialize message header\n";

        protocol::error_response err;
        err.error_code = -1;
        err.error_message = "Invalid message header";
        auto error_bytes = protocol::protocol_serializer::serialize(err);

        network_session->send_packet(std::move(error_bytes));
        return;
    }

    const auto& header = header_result.value();
    if (!header.is_valid()) {
        std::cerr << "Invalid message header magic or version\n";
        return;
    }

    constexpr size_t header_size = sizeof(protocol::message_header);
    std::vector<uint8_t> payload(data.begin() + header_size, data.end());

    std::vector<uint8_t> response;

    switch (header.type) {
        case protocol::message_type::QUERY_REQUEST:
            response = process_query_request(header, payload);
            break;

        case protocol::message_type::BEGIN_TRANSACTION:
        case protocol::message_type::COMMIT_TRANSACTION:
        case protocol::message_type::ROLLBACK_TRANSACTION:
            {
                auto txn_request_result = protocol::protocol_serializer::deserialize_transaction_request(payload);

                protocol::transaction_response txn_response;

                if (!txn_request_result.has_value()) {
                    txn_response.success = false;
                    txn_response.error_message = "Failed to deserialize transaction request";
                } else {
                    auto pool = db_pool_->get_pool(database_types::sqlite);
                    if (!pool) {
                        txn_response.success = false;
                        txn_response.error_message = "No connection pool available";
                    } else {
                        auto conn_result = pool->acquire_connection();
                        if (!conn_result.is_ok()) {
                            txn_response.success = false;
                            txn_response.error_message = "Failed to acquire database connection";
                        } else {
                            auto conn_wrapper = conn_result.value();
                            auto db_conn = std::shared_ptr<database_base>(conn_wrapper->get(), [conn_wrapper](database_base*) {
                            });

                            auto temp_session = std::make_shared<database_session>(
                                "txn_temp_" + std::to_string(header.request_id),
                                db_conn
                            );

                            try {
                                switch (header.type) {
                                    case protocol::message_type::BEGIN_TRANSACTION:
                                        txn_response = temp_session->begin_transaction();
                                        break;
                                    case protocol::message_type::COMMIT_TRANSACTION:
                                        txn_response = temp_session->commit_transaction();
                                        break;
                                    case protocol::message_type::ROLLBACK_TRANSACTION:
                                        txn_response = temp_session->rollback_transaction();
                                        break;
                                    default:
                                        txn_response.success = false;
                                        txn_response.error_message = "Unknown transaction type";
                                        break;
                                }
                            } catch (const std::exception& e) {
                                txn_response.success = false;
                                txn_response.error_message = std::string("Exception during transaction: ") + e.what();
                            }
                        }
                    }
                }

                auto txn_response_bytes = protocol::protocol_serializer::serialize(txn_response);

                protocol::message_header txn_response_header;
                txn_response_header.type = protocol::message_type::TRANSACTION_RESPONSE;
                txn_response_header.request_id = header.request_id;
                txn_response_header.payload_size = static_cast<uint32_t>(txn_response_bytes.size());

                auto txn_header_bytes = protocol::protocol_serializer::serialize_header(txn_response_header);
                txn_header_bytes.insert(txn_header_bytes.end(), txn_response_bytes.begin(), txn_response_bytes.end());

                response = std::move(txn_header_bytes);
            }
            break;

        case protocol::message_type::PING:
            {
                protocol::message_header pong_header = header;
                pong_header.type = protocol::message_type::PONG;
                pong_header.payload_size = 0;
                response = protocol::protocol_serializer::serialize_header(pong_header);
            }
            break;

        default:
            {
                protocol::error_response err;
                err.error_code = -2;
                err.error_message = "Unsupported message type";
                response = protocol::protocol_serializer::serialize(err);
            }
            break;
    }

    if (!response.empty()) {
        network_session->send_packet(std::move(response));
    }
}

std::vector<uint8_t> database_proxy_server::process_query_request(
    const protocol::message_header& header,
    const std::vector<uint8_t>& payload
) {
    auto request_result = protocol::protocol_serializer::deserialize_query_request(payload);
    if (!request_result.has_value()) {
        protocol::error_response err;
        err.error_code = -3;
        err.error_message = "Failed to deserialize query request";
        return protocol::protocol_serializer::serialize(err);
    }

    const auto& request = request_result.value();

    protocol::query_response response;

    auto pool = db_pool_->get_pool(database_types::sqlite);
    if (!pool) {
        response.success = false;
        response.error_message = "No connection pool available for requested database type";
        response.error_code = -4;
    } else {
        auto conn_result = pool->acquire_connection();
        if (!conn_result.is_ok()) {
            response.success = false;
            response.error_message = "Failed to acquire database connection";
            response.error_code = -5;
        } else {
            auto conn_wrapper = conn_result.value();
            auto db_conn = std::shared_ptr<database_base>(conn_wrapper->get(), [conn_wrapper](database_base*) {
            });

            auto temp_session = std::make_shared<database_session>(
                "temp_" + std::to_string(header.request_id),
                db_conn
            );

            try {
                response = temp_session->execute_query(request);
            } catch (const std::exception& e) {
                response.success = false;
                response.error_message = std::string("Exception during query execution: ") + e.what();
                response.error_code = -6;
            }
        }
    }

    auto response_bytes = protocol::protocol_serializer::serialize(response);

    protocol::message_header response_header;
    response_header.type = protocol::message_type::QUERY_RESPONSE;
    response_header.request_id = header.request_id;
    response_header.payload_size = static_cast<uint32_t>(response_bytes.size());

    auto header_bytes = protocol::protocol_serializer::serialize_header(response_header);
    header_bytes.insert(header_bytes.end(), response_bytes.begin(), response_bytes.end());

    return header_bytes;
}

} // namespace database::server
