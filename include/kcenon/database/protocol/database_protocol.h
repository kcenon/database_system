// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <kcenon/database/database_types.h>
#include <kcenon/database/core/result.h>

namespace database::protocol {

/**
 * @enum message_type
 * @brief Database protocol message types
 *
 * Binary protocol for client-server communication.
 * Each message has a type, request ID, and payload.
 */
enum class message_type : uint16_t {
    // Connection management
    CONNECT_REQUEST = 1,
    CONNECT_RESPONSE = 2,
    DISCONNECT = 3,
    PING = 4,
    PONG = 5,

    // Query operations
    QUERY_REQUEST = 10,
    QUERY_RESPONSE = 11,

    // Transaction management
    BEGIN_TRANSACTION = 20,
    COMMIT_TRANSACTION = 21,
    ROLLBACK_TRANSACTION = 22,
    TRANSACTION_RESPONSE = 23,

    // Prepared statements
    PREPARE_STATEMENT = 30,
    EXECUTE_PREPARED = 31,
    CLOSE_PREPARED = 32,

    // Error handling
    ERROR_RESPONSE = 100
};

/**
 * @enum query_operation
 * @brief Type of query operation
 */
enum class query_operation : uint8_t {
    SELECT = 1,
    INSERT = 2,
    UPDATE = 3,
    DELETE = 4,
    CREATE = 5,
    ALTER = 6,
    DROP = 7,
    OTHER = 99
};

/**
 * @struct message_header
 * @brief Common header for all protocol messages
 *
 * Binary layout (12 bytes):
 * - magic: 4 bytes (0xDB01DB01)
 * - version: 2 bytes (1)
 * - type: 2 bytes
 * - request_id: 8 bytes
 * - payload_size: 4 bytes
 */
struct message_header {
    static constexpr uint32_t MAGIC = 0xDB01DB01;
    static constexpr uint16_t PROTOCOL_VERSION = 1;

    uint32_t magic = MAGIC;
    uint16_t version = PROTOCOL_VERSION;
    message_type type;
    uint64_t request_id;
    uint32_t payload_size;

    /**
     * @brief Validate message header
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid() const {
        return magic == MAGIC && version == PROTOCOL_VERSION;
    }
};

/**
 * @struct connect_request
 * @brief Client connection request
 */
struct connect_request {
    std::string database_type;  // "postgresql", "sqlite", etc.
    std::string connection_string;
    std::map<std::string, std::string> options;
};

/**
 * @struct connect_response
 * @brief Server connection response
 */
struct connect_response {
    bool success;
    std::string session_id;
    std::string error_message;
};

/**
 * @struct query_request
 * @brief Query execution request
 */
struct query_request {
    query_operation operation;
    std::string query_string;
    std::vector<std::string> parameters;  // For prepared statements
};

/**
 * @struct query_response
 * @brief Query execution response
 */
struct query_response {
    bool success;

    // For SELECT queries
    std::vector<std::map<std::string, std::string>> rows;
    std::vector<std::string> column_names;

    // For INSERT/UPDATE/DELETE
    uint64_t affected_rows = 0;
    uint64_t last_insert_id = 0;

    // Error information
    std::string error_message;
    int32_t error_code = 0;
};

/**
 * @struct transaction_request
 * @brief Transaction control request
 */
struct transaction_request {
    message_type operation;  // BEGIN, COMMIT, or ROLLBACK
};

/**
 * @struct transaction_response
 * @brief Transaction control response
 */
struct transaction_response {
    bool success;
    std::string error_message;
};

/**
 * @struct error_response
 * @brief Error response message
 */
struct error_response {
    int32_t error_code;
    std::string error_message;
    std::string error_context;
};

/**
 * @class protocol_serializer
 * @brief Serialization/deserialization utilities for protocol messages
 *
 * Provides binary serialization for network transmission.
 * Uses little-endian byte order.
 */
class protocol_serializer {
public:
    /**
     * @brief Serialize message header to bytes
     * @param header Message header
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize_header(const message_header& header);

    /**
     * @brief Deserialize message header from bytes
     * @param data Serialized bytes
     * @return Deserialized header or error
     */
    static kcenon::common::Result<message_header> deserialize_header(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize connect request
     * @param request Connect request
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const connect_request& request);

    /**
     * @brief Deserialize connect request
     * @param data Serialized bytes
     * @return Deserialized request or error
     */
    static kcenon::common::Result<connect_request> deserialize_connect_request(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize connect response
     * @param response Connect response
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const connect_response& response);

    /**
     * @brief Deserialize connect response
     * @param data Serialized bytes
     * @return Deserialized response or error
     */
    static kcenon::common::Result<connect_response> deserialize_connect_response(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize query request
     * @param request Query request
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const query_request& request);

    /**
     * @brief Deserialize query request
     * @param data Serialized bytes
     * @return Deserialized request or error
     */
    static kcenon::common::Result<query_request> deserialize_query_request(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize query response
     * @param response Query response
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const query_response& response);

    /**
     * @brief Deserialize query response
     * @param data Serialized bytes
     * @return Deserialized response or error
     */
    static kcenon::common::Result<query_response> deserialize_query_response(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize transaction request
     * @param request Transaction request
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const transaction_request& request);

    /**
     * @brief Deserialize transaction request
     * @param data Serialized bytes
     * @return Deserialized request or error
     */
    static kcenon::common::Result<transaction_request> deserialize_transaction_request(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize error response
     * @param response Error response
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const error_response& response);

    /**
     * @brief Deserialize error response
     * @param data Serialized bytes
     * @return Deserialized response or error
     */
    static kcenon::common::Result<error_response> deserialize_error_response(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize transaction response
     * @param response Transaction response
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize(const transaction_response& response);

    /**
     * @brief Deserialize transaction response
     * @param data Serialized bytes
     * @return Deserialized response or error
     */
    static kcenon::common::Result<transaction_response> deserialize_transaction_response(const std::vector<uint8_t>& data);

private:
    // Helper methods for primitive types
    static void write_uint8(std::vector<uint8_t>& buffer, uint8_t value);
    static void write_uint16(std::vector<uint8_t>& buffer, uint16_t value);
    static void write_uint32(std::vector<uint8_t>& buffer, uint32_t value);
    static void write_uint64(std::vector<uint8_t>& buffer, uint64_t value);
    static void write_string(std::vector<uint8_t>& buffer, const std::string& value);

    static uint8_t read_uint8(const std::vector<uint8_t>& buffer, size_t& offset);
    static uint16_t read_uint16(const std::vector<uint8_t>& buffer, size_t& offset);
    static uint32_t read_uint32(const std::vector<uint8_t>& buffer, size_t& offset);
    static uint64_t read_uint64(const std::vector<uint8_t>& buffer, size_t& offset);
    static std::string read_string(const std::vector<uint8_t>& buffer, size_t& offset);
};

} // namespace database::protocol
