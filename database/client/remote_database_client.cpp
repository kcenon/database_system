/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "remote_database_client.h"
#include <sstream>

namespace database::client {

remote_database_client::remote_database_client() = default;

remote_database_client::~remote_database_client() { shutdown(); }

database_types remote_database_client::type() const {
  return database_types::REMOTE;
}

kcenon::common::VoidResult
remote_database_client::initialize(const core::connection_config &config) {
  if (initialized_.exchange(true)) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::invalid_argument),
        "Client already initialized",
        "remote_database_client"};
  }

  config_ = config;

  try {
    // Create resilient_client with auto-reconnect
    network_client_ = std::make_shared<network_system::utils::resilient_client>(
        "db_client_" + config.database, config.host, config.port,
        3,                      // max retries
        std::chrono::seconds(1) // initial backoff
    );

    // Set up reconnection callback
    network_client_->set_reconnect_callback([this](size_t attempt) {
      if (logger_) {
        logger_->log_connection_event("reconnecting",
                                      "attempt " + std::to_string(attempt));
      }
    });

    network_client_->set_disconnect_callback([this]() {
      if (logger_) {
        logger_->log_connection_event("disconnected", "from database server");
      }
    });

    // Set up receive callback for async responses
    auto underlying_client = network_client_->get_client();
    if (underlying_client) {
      underlying_client->set_receive_callback(
          [this](const auto &data) { handle_response(data); });
    }

    // Connect to server
    auto result = network_client_->connect();
    if (result.is_err()) {
      initialized_ = false;
      return kcenon::common::error_info{
          static_cast<int>(database::error_code::unknown_error),
          "Failed to connect to database server: " + result.error().message,
          "remote_database_client"};
    }

    if (logger_) {
      logger_->log_connection_event(
          "connected", config.host + ":" + std::to_string(config.port));
    }
    return kcenon::common::ok();
  } catch (const std::exception &e) {
    initialized_ = false;
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        std::string("Exception during initialization: ") + e.what(),
        "remote_database_client"};
  }
}

kcenon::common::VoidResult remote_database_client::shutdown() {
  if (!initialized_.exchange(false)) {
    return kcenon::common::ok(); // Already shut down
  }

  // Cancel all pending requests
  {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    for (auto &[id, pending] : pending_responses_) {
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
      if (logger_) {
        logger_->log_error("disconnect", result.error().message, "");
      }
    }
  }

  if (logger_) {
    logger_->log_connection_event("disconnected", "graceful shutdown");
  }

  return kcenon::common::ok();
}

bool remote_database_client::is_initialized() const {
  return initialized_.load();
}

kcenon::common::Result<uint64_t>
remote_database_client::insert_query(const std::string &query_string) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  // Create query request
  protocol::query_request request;
  request.operation = protocol::query_operation::INSERT;
  request.query_string = query_string;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::QUERY_REQUEST, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  // Deserialize response
  auto query_response =
      protocol::protocol_serializer::deserialize_query_response(
          response_result.value());
  if (query_response.is_err()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to deserialize query response",
        "remote_database_client"};
  }

  if (!query_response.value().success) {
    return kcenon::common::error_info{
        query_response.value().error_code,
        query_response.value().error_message,
        "remote_database_client"};
  }

  return query_response.value().affected_rows;
}

kcenon::common::Result<uint64_t>
remote_database_client::update_query(const std::string &query_string) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  protocol::query_request request;
  request.operation = protocol::query_operation::UPDATE;
  request.query_string = query_string;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::QUERY_REQUEST, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  auto query_response =
      protocol::protocol_serializer::deserialize_query_response(
          response_result.value());
  if (query_response.is_err()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to deserialize query response",
        "remote_database_client"};
  }

  if (!query_response.value().success) {
    return kcenon::common::error_info{
        query_response.value().error_code,
        query_response.value().error_message,
        "remote_database_client"};
  }

  return query_response.value().affected_rows;
}

kcenon::common::Result<uint64_t>
remote_database_client::delete_query(const std::string &query_string) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  protocol::query_request request;
  request.operation = protocol::query_operation::DELETE;
  request.query_string = query_string;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::QUERY_REQUEST, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  auto query_response =
      protocol::protocol_serializer::deserialize_query_response(
          response_result.value());
  if (query_response.is_err()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to deserialize query response",
        "remote_database_client"};
  }

  if (!query_response.value().success) {
    return kcenon::common::error_info{
        query_response.value().error_code,
        query_response.value().error_message,
        "remote_database_client"};
  }

  return query_response.value().affected_rows;
}

kcenon::common::Result<core::database_result>
remote_database_client::select_query(const std::string &query_string) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  protocol::query_request request;
  request.operation = protocol::query_operation::SELECT;
  request.query_string = query_string;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::QUERY_REQUEST, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  auto query_response =
      protocol::protocol_serializer::deserialize_query_response(
          response_result.value());
  if (query_response.is_err()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to deserialize query response",
        "remote_database_client"};
  }

  if (!query_response.value().success) {
    return kcenon::common::error_info{
        query_response.value().error_code,
        query_response.value().error_message,
        "remote_database_client"};
  }

  // Convert protocol rows to database_result
  core::database_result result;
  for (const auto &row_map : query_response.value().rows) {
    core::database_row row;
    for (const auto &[key, value_str] : row_map) {
      row[key] = value_str; // Convert string to database_value
    }
    result.push_back(row);
  }

  return result;
}

kcenon::common::VoidResult
remote_database_client::execute_query(const std::string &query_string) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  protocol::query_request request;
  request.operation = protocol::query_operation::OTHER;
  request.query_string = query_string;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::QUERY_REQUEST, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  auto query_response =
      protocol::protocol_serializer::deserialize_query_response(
          response_result.value());
  if (query_response.is_err()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to deserialize query response",
        "remote_database_client"};
  }

  if (!query_response.value().success) {
    return kcenon::common::error_info{
        query_response.value().error_code,
        query_response.value().error_message,
        "remote_database_client"};
  }

  return kcenon::common::ok();
}

kcenon::common::VoidResult remote_database_client::begin_transaction() {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  if (in_transaction_.exchange(true)) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Transaction already in progress",
        "remote_database_client"};
  }

  protocol::transaction_request request;
  request.operation = protocol::message_type::BEGIN_TRANSACTION;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::BEGIN_TRANSACTION, payload);

  if (response_result.is_err()) {
    in_transaction_ = false;
    return response_result.error();
  }

  // Parse transaction_response
  auto txn_response_result =
      protocol::protocol_serializer::deserialize_transaction_response(
          response_result.value());
  if (txn_response_result.is_err()) {
    in_transaction_ = false;
    return txn_response_result.error();
  }

  auto txn_response = txn_response_result.value();
  if (!txn_response.success) {
    in_transaction_ = false;
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        txn_response.error_message,
        "remote_database_client"};
  }

  return kcenon::common::ok();
}

kcenon::common::VoidResult remote_database_client::commit_transaction() {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  if (!in_transaction_.exchange(false)) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "No active transaction",
        "remote_database_client"};
  }

  protocol::transaction_request request;
  request.operation = protocol::message_type::COMMIT_TRANSACTION;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::COMMIT_TRANSACTION, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  if (logger_) {
    logger_->log_transaction("commit", true, "");
  }
  return kcenon::common::ok();
}

kcenon::common::VoidResult remote_database_client::rollback_transaction() {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
  }

  if (!in_transaction_.exchange(false)) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "No active transaction",
        "remote_database_client"};
  }

  protocol::transaction_request request;
  request.operation = protocol::message_type::ROLLBACK_TRANSACTION;

  auto payload = protocol::protocol_serializer::serialize(request);
  auto response_result =
      send_request(protocol::message_type::ROLLBACK_TRANSACTION, payload);

  if (response_result.is_err()) {
    return response_result.error();
  }

  if (logger_) {
    logger_->log_transaction("rollback", true, "");
  }
  return kcenon::common::ok();
}

bool remote_database_client::in_transaction() const {
  return in_transaction_.load();
}

std::string remote_database_client::last_error() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_;
}

std::map<std::string, std::string>
remote_database_client::connection_info() const {
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

std::future<kcenon::common::Result<core::database_result>>
remote_database_client::execute_async(const std::string &query_string) {
  auto promise =
      std::make_shared<std::promise<kcenon::common::Result<core::database_result>>>();
  auto future = promise->get_future();

  // Execute async using std::async
  // Note: Network request itself is already asynchronous via resilient_client
  (void)std::async(std::launch::async, [this, query_string, promise]() {
    auto result = select_query(query_string);
    promise->set_value(result);
  });

  return future;
}

kcenon::common::Result<std::vector<uint8_t>>
remote_database_client::send_request(protocol::message_type request_type,
                                     const std::vector<uint8_t> &payload,
                                     std::chrono::milliseconds timeout) {
  if (!is_initialized()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Client not initialized",
        "remote_database_client"};
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

    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Failed to send request: " + send_result.error().message,
        "remote_database_client"};
  }

  // Wait for response with timeout
  auto status = response_future.wait_for(timeout);
  if (status == std::future_status::timeout) {
    // Remove pending request
    std::lock_guard<std::mutex> lock(requests_mutex_);
    pending_responses_.erase(req_id);

    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Request timeout",
        "remote_database_client"};
  }

  // Get response
  auto response_data = response_future.get();
  if (response_data.empty()) {
    return kcenon::common::error_info{
        static_cast<int>(database::error_code::unknown_error),
        "Empty response received",
        "remote_database_client"};
  }

  return response_data;
}

void remote_database_client::handle_response(
    const std::vector<uint8_t> &message_data) {
  // Deserialize message header
  auto header_result =
      protocol::protocol_serializer::deserialize_header(message_data);
  if (header_result.is_err()) {
    if (logger_) {
      logger_->log_error("handle_response",
                         "Failed to deserialize response header", "");
    }
    return;
  }

  const auto &header = header_result.value();
  uint64_t request_id = header.request_id;

  // Extract payload (skip header)
  constexpr size_t header_size = sizeof(protocol::message_header);
  std::vector<uint8_t> payload(message_data.begin() + header_size,
                               message_data.end());

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
}

uint64_t remote_database_client::next_request_id() {
  return next_request_id_.fetch_add(1);
}

} // namespace database::client
