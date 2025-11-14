/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "database_protocol.h"
#include <cstring>

namespace database::protocol {

// Stub implementations - TODO: Implement full serialization

std::vector<uint8_t> protocol_serializer::serialize_header(const message_header& header) {
    std::vector<uint8_t> buffer;
    buffer.reserve(20);  // Header size

    write_uint32(buffer, header.magic);
    write_uint16(buffer, header.version);
    write_uint16(buffer, static_cast<uint16_t>(header.type));
    write_uint64(buffer, header.request_id);
    write_uint32(buffer, header.payload_size);

    return buffer;
}

result<message_header> protocol_serializer::deserialize_header(const std::vector<uint8_t>& data) {
    if (data.size() < 20) {
        return error{error_code::invalid_argument, "Header too small"};
    }

    message_header header;
    size_t offset = 0;

    header.magic = read_uint32(data, offset);
    header.version = read_uint16(data, offset);
    header.type = static_cast<message_type>(read_uint16(data, offset));
    header.request_id = read_uint64(data, offset);
    header.payload_size = read_uint32(data, offset);

    if (!header.is_valid()) {
        return error{error_code::invalid_argument, "Invalid header magic or version"};
    }

    return header;
}

// Implementations for request/response serialization
std::vector<uint8_t> protocol_serializer::serialize(const connect_request& request) {
    std::vector<uint8_t> buffer;
    write_string(buffer, request.database_type);
    write_string(buffer, request.connection_string);

    // Serialize options map
    write_uint32(buffer, static_cast<uint32_t>(request.options.size()));
    for (const auto& [key, value] : request.options) {
        write_string(buffer, key);
        write_string(buffer, value);
    }

    return buffer;
}

result<connect_request> protocol_serializer::deserialize_connect_request(const std::vector<uint8_t>& data) {
    connect_request request;
    size_t offset = 0;
    if (data.size() < 4) {
        return error{error_code::invalid_argument, "Data too small"};
    }
    request.database_type = read_string(data, offset);
    request.connection_string = read_string(data, offset);

    // Deserialize options map
    if (offset + 4 > data.size()) {
        return error{error_code::invalid_argument, "Data too small for options"};
    }
    uint32_t options_count = read_uint32(data, offset);
    for (uint32_t i = 0; i < options_count; ++i) {
        if (offset >= data.size()) {
            return error{error_code::invalid_argument, "Data truncated in options"};
        }
        std::string key = read_string(data, offset);
        std::string value = read_string(data, offset);
        request.options[key] = value;
    }

    return request;
}

std::vector<uint8_t> protocol_serializer::serialize(const query_request& request) {
    std::vector<uint8_t> buffer;
    write_uint8(buffer, static_cast<uint8_t>(request.operation));
    write_string(buffer, request.query_string);

    // Serialize parameters vector
    write_uint32(buffer, static_cast<uint32_t>(request.parameters.size()));
    for (const auto& param : request.parameters) {
        write_string(buffer, param);
    }

    return buffer;
}

result<query_request> protocol_serializer::deserialize_query_request(const std::vector<uint8_t>& data) {
    query_request request;
    size_t offset = 0;
    if (data.empty()) {
        return error{error_code::invalid_argument, "Empty data"};
    }
    request.operation = static_cast<query_operation>(read_uint8(data, offset));
    request.query_string = read_string(data, offset);

    // Deserialize parameters vector
    if (offset + 4 > data.size()) {
        return error{error_code::invalid_argument, "Data too small for parameters"};
    }
    uint32_t param_count = read_uint32(data, offset);
    request.parameters.reserve(param_count);
    for (uint32_t i = 0; i < param_count; ++i) {
        if (offset >= data.size()) {
            return error{error_code::invalid_argument, "Data truncated in parameters"};
        }
        request.parameters.push_back(read_string(data, offset));
    }

    return request;
}

std::vector<uint8_t> protocol_serializer::serialize(const query_response& response) {
    std::vector<uint8_t> buffer;
    write_uint8(buffer, response.success ? 1 : 0);
    write_uint64(buffer, response.affected_rows);
    write_uint64(buffer, response.last_insert_id);
    write_uint32(buffer, static_cast<uint32_t>(response.error_code));
    write_string(buffer, response.error_message);

    // Serialize column names
    write_uint32(buffer, static_cast<uint32_t>(response.column_names.size()));
    for (const auto& column : response.column_names) {
        write_string(buffer, column);
    }

    // Serialize rows (vector of maps)
    write_uint32(buffer, static_cast<uint32_t>(response.rows.size()));
    for (const auto& row : response.rows) {
        // Each row is a map<string, string>
        write_uint32(buffer, static_cast<uint32_t>(row.size()));
        for (const auto& [key, value] : row) {
            write_string(buffer, key);
            write_string(buffer, value);
        }
    }

    return buffer;
}

result<query_response> protocol_serializer::deserialize_query_response(const std::vector<uint8_t>& data) {
    query_response response;
    size_t offset = 0;
    if (data.empty()) {
        return error{error_code::invalid_argument, "Empty data"};
    }
    response.success = (read_uint8(data, offset) != 0);
    response.affected_rows = read_uint64(data, offset);
    response.last_insert_id = read_uint64(data, offset);
    response.error_code = static_cast<int32_t>(read_uint32(data, offset));
    response.error_message = read_string(data, offset);

    // Deserialize column names
    if (offset + 4 > data.size()) {
        return error{error_code::invalid_argument, "Data too small for column names"};
    }
    uint32_t column_count = read_uint32(data, offset);
    response.column_names.reserve(column_count);
    for (uint32_t i = 0; i < column_count; ++i) {
        if (offset >= data.size()) {
            return error{error_code::invalid_argument, "Data truncated in column names"};
        }
        response.column_names.push_back(read_string(data, offset));
    }

    // Deserialize rows
    if (offset + 4 > data.size()) {
        return error{error_code::invalid_argument, "Data too small for rows"};
    }
    uint32_t row_count = read_uint32(data, offset);
    response.rows.reserve(row_count);
    for (uint32_t i = 0; i < row_count; ++i) {
        if (offset + 4 > data.size()) {
            return error{error_code::invalid_argument, "Data too small for row"};
        }
        uint32_t field_count = read_uint32(data, offset);
        std::map<std::string, std::string> row;
        for (uint32_t j = 0; j < field_count; ++j) {
            if (offset >= data.size()) {
                return error{error_code::invalid_argument, "Data truncated in row fields"};
            }
            std::string key = read_string(data, offset);
            std::string value = read_string(data, offset);
            row[key] = value;
        }
        response.rows.push_back(std::move(row));
    }

    return response;
}

std::vector<uint8_t> protocol_serializer::serialize(const connect_response& response) {
    std::vector<uint8_t> buffer;
    write_uint8(buffer, response.success ? 1 : 0);
    write_string(buffer, response.session_id);
    write_string(buffer, response.error_message);
    return buffer;
}

result<connect_response> protocol_serializer::deserialize_connect_response(const std::vector<uint8_t>& data) {
    connect_response response;
    size_t offset = 0;
    if (data.empty()) {
        return error{error_code::invalid_argument, "Empty data"};
    }
    response.success = (read_uint8(data, offset) != 0);
    response.session_id = read_string(data, offset);
    response.error_message = read_string(data, offset);
    return response;
}

std::vector<uint8_t> protocol_serializer::serialize(const transaction_request& request) {
    std::vector<uint8_t> buffer;
    write_uint16(buffer, static_cast<uint16_t>(request.operation));
    return buffer;
}

result<transaction_request> protocol_serializer::deserialize_transaction_request(const std::vector<uint8_t>& data) {
    transaction_request request;
    size_t offset = 0;
    if (data.size() < 2) {
        return error{error_code::invalid_argument, "Data too small"};
    }
    request.operation = static_cast<message_type>(read_uint16(data, offset));
    return request;
}

std::vector<uint8_t> protocol_serializer::serialize(const error_response& response) {
    std::vector<uint8_t> buffer;
    write_uint32(buffer, response.error_code);
    write_string(buffer, response.error_message);
    write_string(buffer, response.error_context);
    return buffer;
}

result<error_response> protocol_serializer::deserialize_error_response(const std::vector<uint8_t>& data) {
    error_response response;
    size_t offset = 0;
    if (data.size() < 4) {
        return error{error_code::invalid_argument, "Data too small"};
    }
    response.error_code = static_cast<int32_t>(read_uint32(data, offset));
    response.error_message = read_string(data, offset);
    response.error_context = read_string(data, offset);
    return response;
}

std::vector<uint8_t> protocol_serializer::serialize(const transaction_response& response) {
    std::vector<uint8_t> buffer;
    write_uint8(buffer, response.success ? 1 : 0);
    write_string(buffer, response.error_message);
    return buffer;
}

result<transaction_response> protocol_serializer::deserialize_transaction_response(const std::vector<uint8_t>& data) {
    transaction_response response;
    size_t offset = 0;
    if (data.empty()) {
        return error{error_code::invalid_argument, "Empty data"};
    }
    response.success = (read_uint8(data, offset) != 0);
    response.error_message = read_string(data, offset);
    return response;
}

// Helper methods for primitive types (little-endian)
void protocol_serializer::write_uint8(std::vector<uint8_t>& buffer, uint8_t value) {
    buffer.push_back(value);
}

void protocol_serializer::write_uint16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void protocol_serializer::write_uint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void protocol_serializer::write_uint64(std::vector<uint8_t>& buffer, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void protocol_serializer::write_string(std::vector<uint8_t>& buffer, const std::string& value) {
    write_uint32(buffer, static_cast<uint32_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

uint8_t protocol_serializer::read_uint8(const std::vector<uint8_t>& buffer, size_t& offset) {
    return buffer[offset++];
}

uint16_t protocol_serializer::read_uint16(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint16_t value = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
    offset += 2;
    return value;
}

uint32_t protocol_serializer::read_uint32(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t value = buffer[offset] | (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                     (static_cast<uint32_t>(buffer[offset + 2]) << 16) |
                     (static_cast<uint32_t>(buffer[offset + 3]) << 24);
    offset += 4;
    return value;
}

uint64_t protocol_serializer::read_uint64(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(buffer[offset + i]) << (i * 8));
    }
    offset += 8;
    return value;
}

std::string protocol_serializer::read_string(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t length = read_uint32(buffer, offset);
    if (length == 0) {
        return "";
    }
    std::string value(reinterpret_cast<const char*>(&buffer[offset]), length);
    offset += length;
    return value;
}

} // namespace database::protocol
