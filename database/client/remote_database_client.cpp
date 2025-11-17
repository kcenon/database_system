/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "remote_database_client.h"
#include <iostream>
#include <sstream>

namespace database::client {

remote_database_client::remote_database_client() = default;

remote_database_client::~remote_database_client() {
    shutdown();
}

database_types remote_database_client::type() const {
    return database_types::REMOTE;
}

database::result<void> remote_database_client::initialize(const core::connection_config& config) {
    if (initialized_.exchange(true)) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::invalid_argument), "Client already initialized"));
    }

    config_ = config;

    try {
        // Create resilient_client with auto-reconnect
        network_client_ = std::make_shared<network_system::utils::resilient_client>(
            "db_client_" + config.database,
            config.host,
            config.port,
            3,  // max retries
            std::chrono::seconds(1)  // initial backoff
        );

        // Set up reconnection callback
        network_client_->set_reconnect_callback([](size_t attempt) {
            std::cout << "Reconnecting to database server (attempt " << attempt << ")\n";
        });

        network_client_->set_disconnect_callback([]() {
            std::cerr << "Disconnected from database server\n";
        });

        // Set up receive callback for async responses
        auto underlying_client = network_client_->get_client();
        if (underlying_client) {
            underlying_client->set_receive_callback([this](const auto& data) {
                handle_response(data);
            });
        }

        // Connect to server
        auto result = network_client_->connect();
        if (result.is_err()) {
            initialized_ = false;
            return database::result<void>::err(database::error(
                database::error_code::unknown_error,
                "Failed to connect to database server: " + result.error().message
            ));
        }

        std::cout << "Remote database client connected to " << config.host << ":" << config.port << "\n";
        return database::result<void>::ok();
    } catch (const std::exception& e) {
        initialized_ = false;
        return database::result<void>::err(database::error(
            database::error_code::unknown_error,
            std::string("Exception during initialization: ") + e.what()
        ));
    }
#else
    std::cout << "Remote database client connecting to " << config.host << ":" << config.port
              << " (stub implementation)\n";
    return database::result<void>::ok();
}

database::result<void> remote_database_client::shutdown() {
    if (!initialized_.exchange(false)) {
        return database::result<void>::ok();  // Already shut down
    }

    // Cancel all pending requests
    {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        for (auto& [id, pending] : pending_responses_) {
            try {
                std::vector<uint8_t> empty;
                pending.promise.set_value(std::move(empty));
            } catch (...) {
                // Promise may already be set
            }
        }
        pending_responses_.clear();
    }

    // Disconnect from network
    if (network_client_) {
        auto result = network_client_->disconnect();
        if (result.is_err()) {
            std::cerr << "Failed to disconnect: " << result.error().message << "\n";
        }
    }

    std::cout << "Remote database client disconnected\n";

    return database::result<void>::ok();
}

bool remote_database_client::is_initialized() const {
    return initialized_.load();
}

database::result<uint64_t> remote_database_client::insert_query(const std::string& query_string) {
    if (!is_initialized()) {
        return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    // Create query request
    protocol::query_request request;
    request.operation = protocol::query_operation::INSERT;
    request.query_string = query_string;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::QUERY_REQUEST, payload);

    if (!response_result.has_value()) {
        return database::result<uint64_t>::err(response_result.get_error());
    }

    // Deserialize response
    auto query_response = protocol::protocol_serializer::deserialize_query_response(response_result.value());
    if (!query_response.has_value()) {
        return database::result<uint64_t>::err(database::error(
            database::error_code::unknown_error,
            "Failed to deserialize query response"
        ));
    }

    if (!query_response.value().success) {
        return database::result<uint64_t>::err(database::error(
            static_cast<database::error_code>(query_response.value().error_code),
            query_response.value().error_message
        ));
    }

    return database::result<uint64_t>::ok(query_response.value().affected_rows);
#else
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = "Stub implementation: INSERT not supported";
    return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::not_implemented), last_error_));
}

database::result<uint64_t> remote_database_client::update_query(const std::string& query_string) {
    if (!is_initialized()) {
        return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    protocol::query_request request;
    request.operation = protocol::query_operation::UPDATE;
    request.query_string = query_string;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::QUERY_REQUEST, payload);

    if (!response_result.has_value()) {
        return database::result<uint64_t>::err(response_result.get_error());
    }

    auto query_response = protocol::protocol_serializer::deserialize_query_response(response_result.value());
    if (!query_response.has_value()) {
        return database::result<uint64_t>::err(database::error(
            database::error_code::unknown_error,
            "Failed to deserialize query response"
        ));
    }

    if (!query_response.value().success) {
        return database::result<uint64_t>::err(database::error(
            static_cast<database::error_code>(query_response.value().error_code),
            query_response.value().error_message
        ));
    }

    return database::result<uint64_t>::ok(query_response.value().affected_rows);
#else
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = "Stub implementation: UPDATE not supported";
    return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::not_implemented), last_error_));
}

database::result<uint64_t> remote_database_client::delete_query(const std::string& query_string) {
    if (!is_initialized()) {
        return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    protocol::query_request request;
    request.operation = protocol::query_operation::DELETE;
    request.query_string = query_string;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::QUERY_REQUEST, payload);

    if (!response_result.has_value()) {
        return database::result<uint64_t>::err(response_result.get_error());
    }

    auto query_response = protocol::protocol_serializer::deserialize_query_response(response_result.value());
    if (!query_response.has_value()) {
        return database::result<uint64_t>::err(database::error(
            database::error_code::unknown_error,
            "Failed to deserialize query response"
        ));
    }

    if (!query_response.value().success) {
        return database::result<uint64_t>::err(database::error(
            static_cast<database::error_code>(query_response.value().error_code),
            query_response.value().error_message
        ));
    }

    return database::result<uint64_t>::ok(query_response.value().affected_rows);
#else
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = "Stub implementation: DELETE not supported";
    return database::result<uint64_t>::err(database::error(static_cast<int>(database::error_code::not_implemented), last_error_));
}

database::result<core::database_result> remote_database_client::select_query(const std::string& query_string) {
    if (!is_initialized()) {
        return database::result<core::database_result>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    protocol::query_request request;
    request.operation = protocol::query_operation::SELECT;
    request.query_string = query_string;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::QUERY_REQUEST, payload);

    if (!response_result.has_value()) {
        return database::result<core::database_result>::err(response_result.get_error());
    }

    auto query_response = protocol::protocol_serializer::deserialize_query_response(response_result.value());
    if (!query_response.has_value()) {
        return database::result<core::database_result>::err(database::error(
            database::error_code::unknown_error,
            "Failed to deserialize query response"
        ));
    }

    if (!query_response.value().success) {
        return database::result<core::database_result>::err(database::error(
            static_cast<database::error_code>(query_response.value().error_code),
            query_response.value().error_message
        ));
    }

    // Convert protocol rows to database_result
    core::database_result result;
    for (const auto& row_map : query_response.value().rows) {
        core::database_row row;
        for (const auto& [key, value_str] : row_map) {
            row[key] = value_str;  // Convert string to database_value
        }
        result.push_back(row);
    }

    return database::result<core::database_result>::ok(std::move(result));
#else
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = "Stub implementation: SELECT not supported";
    return database::result<core::database_result>::err(database::error(static_cast<int>(database::error_code::not_implemented), last_error_));
}

database::result<void> remote_database_client::execute_query(const std::string& query_string) {
    if (!is_initialized()) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    protocol::query_request request;
    request.operation = protocol::query_operation::OTHER;
    request.query_string = query_string;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::QUERY_REQUEST, payload);

    if (!response_result.has_value()) {
        return database::result<void>::err(response_result.get_error());
    }

    auto query_response = protocol::protocol_serializer::deserialize_query_response(response_result.value());
    if (!query_response.has_value()) {
        return database::result<void>::err(database::error(
            database::error_code::unknown_error,
            "Failed to deserialize query response"
        ));
    }

    if (!query_response.value().success) {
        return database::result<void>::err(database::error(
            static_cast<database::error_code>(query_response.value().error_code),
            query_response.value().error_message
        ));
    }

    return database::result<void>::ok();
#else
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = "Stub implementation: execute_query not supported";
    return database::result<void>::err(database::error(static_cast<int>(database::error_code::not_implemented), last_error_));
}

database::result<void> remote_database_client::begin_transaction() {
    if (!is_initialized()) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    if (in_transaction_.exchange(true)) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Transaction already in progress"));
    }

    protocol::transaction_request request;
    request.operation = protocol::message_type::BEGIN_TRANSACTION;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::BEGIN_TRANSACTION, payload);

    if (!response_result.has_value()) {
        in_transaction_ = false;
        return database::result<void>::err(response_result.get_error());
    }

    // Parse transaction_response
    auto txn_response_result = protocol::protocol_serializer::deserialize_transaction_response(response_result.value());
    if (txn_response_result.is_err()) {
        in_transaction_ = false;
        return database::result<void>::err(txn_response_result.get_error());
    }

    auto txn_response = txn_response_result.value();
    if (!txn_response.success) {
        in_transaction_ = false;
        return database::result<void>::err(database::error(
            database::error_code::unknown_error,
            txn_response.error_message
        ));
    }

    return database::result<void>::ok();
#else
    std::cout << "Begin transaction (stub)\n";
    return database::result<void>::ok();
}

database::result<void> remote_database_client::commit_transaction() {
    if (!is_initialized()) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    if (!in_transaction_.exchange(false)) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "No active transaction"));
    }

    protocol::transaction_request request;
    request.operation = protocol::message_type::COMMIT_TRANSACTION;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::COMMIT_TRANSACTION, payload);

    if (!response_result.has_value()) {
        return database::result<void>::err(response_result.get_error());
    }

    std::cout << "Commit transaction\n";
    return database::result<void>::ok();
#else
    std::cout << "Commit transaction (stub)\n";
    return database::result<void>::ok();
}

database::result<void> remote_database_client::rollback_transaction() {
    if (!is_initialized()) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    if (!in_transaction_.exchange(false)) {
        return database::result<void>::err(database::error(static_cast<int>(database::error_code::unknown_error), "No active transaction"));
    }

    protocol::transaction_request request;
    request.operation = protocol::message_type::ROLLBACK_TRANSACTION;

    auto payload = protocol::protocol_serializer::serialize(request);
    auto response_result = send_request(protocol::message_type::ROLLBACK_TRANSACTION, payload);

    if (!response_result.has_value()) {
        return database::result<void>::err(response_result.get_error());
    }

    std::cout << "Rollback transaction\n";
    return database::result<void>::ok();
#else
    std::cout << "Rollback transaction (stub)\n";
    return database::result<void>::ok();
}

bool remote_database_client::in_transaction() const {
    return in_transaction_.load();
}

std::string remote_database_client::last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

std::map<std::string, std::string> remote_database_client::connection_info() const {
    std::map<std::string, std::string> info;
    info["type"] = "remote";
    info["host"] = config_.host;
    info["port"] = std::to_string(config_.port);
    info["database"] = config_.database;
    info["initialized"] = is_initialized() ? "true" : "false";
    info["in_transaction"] = in_transaction() ? "true" : "false";

    if (network_client_) {
        info["connected"] = network_client_->is_connected() ? "true" : "false";
    }

    return info;
}

std::future<database::result<core::database_result>> remote_database_client::execute_async(
    const std::string& query_string
) {
    auto promise = std::make_shared<std::promise<database::result<core::database_result>>>();
    auto future = promise->get_future();

    // Execute async using std::async
    // Note: Network request itself is already asynchronous via resilient_client
    std::async(std::launch::async, [this, query_string, promise]() {
        auto result = select_query(query_string);
        promise->set_value(result);
    });

    return future;
}

database::result<std::vector<uint8_t>> remote_database_client::send_request(
    protocol::message_type request_type,
    const std::vector<uint8_t>& payload,
    std::chrono::milliseconds timeout
) {
    if (!is_initialized()) {
        return database::result<std::vector<uint8_t>>::err(database::error(static_cast<int>(database::error_code::unknown_error), "Client not initialized"));
    }

    // Generate request ID
    uint64_t req_id = next_request_id();

    // Create message header
    protocol::message_header header;
    header.type = request_type;
    header.request_id = req_id;
    header.payload_size = static_cast<uint32_t>(payload.size());

    // Serialize header + payload
    auto header_bytes = protocol::protocol_serializer::serialize_header(header);
    header_bytes.insert(header_bytes.end(), payload.begin(), payload.end());

    // Create promise/future for response
    pending_response pending;
    pending.deadline = std::chrono::steady_clock::now() + timeout;
    auto response_future = pending.promise.get_future();

    {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_responses_[req_id] = std::move(pending);
    }

    // Send request
    auto send_result = network_client_->send_with_retry(std::move(header_bytes));
    if (send_result.is_err()) {
        // Remove pending request
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_responses_.erase(req_id);

        return database::result<std::vector<uint8_t>>::err(database::error(
            database::error_code::unknown_error,
            "Failed to send request: " + send_result.error().message
        ));
    }

    // Wait for response with timeout
    auto status = response_future.wait_for(timeout);
    if (status == std::future_status::timeout) {
        // Remove pending request
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_responses_.erase(req_id);

        return database::result<std::vector<uint8_t>>::err(database::error(
            database::error_code::unknown_error,
            "Request timeout"
        ));
    }

    // Get response
    auto response_data = response_future.get();
    if (response_data.empty()) {
        return database::result<std::vector<uint8_t>>::err(database::error(
            database::error_code::unknown_error,
            "Empty response received"
        ));
    }

    return database::result<std::vector<uint8_t>>::ok(std::move(response_data));
#else
    return database::result<std::vector<uint8_t>>::err(database::error(static_cast<int>(database::error_code::not_implemented), "Stub implementation"));
}

void remote_database_client::handle_response(const std::vector<uint8_t>& message_data) {
    // Deserialize message header
    auto header_result = protocol::protocol_serializer::deserialize_header(message_data);
    if (!header_result.has_value()) {
        std::cerr << "Failed to deserialize response header\n";
        return;
    }

    const auto& header = header_result.value();
    uint64_t request_id = header.request_id;

    // Extract payload (skip header)
    constexpr size_t header_size = sizeof(protocol::message_header);
    std::vector<uint8_t> payload(message_data.begin() + header_size, message_data.end());

    // Find corresponding pending request
    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto it = pending_responses_.find(request_id);
    if (it != pending_responses_.end()) {
        try {
            it->second.promise.set_value(std::move(payload));
        } catch (...) {
            // Promise may already be set
        }
        pending_responses_.erase(it);
    }
#else
    std::cout << "Received response: " << message_data.size() << " bytes (stub)\n";
}

uint64_t remote_database_client::next_request_id() {
    return next_request_id_.fetch_add(1);
}

} // namespace database::client
