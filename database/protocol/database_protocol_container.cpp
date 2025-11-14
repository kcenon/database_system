/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#ifdef USE_CONTAINER_SYSTEM

#include "database_protocol_container.h"
#include <core/value_types.h>

namespace database::protocol {

using namespace container_module;

std::vector<uint8_t> container_protocol_serializer::serialize_container(
    const query_request& request) {
    // Create container
    auto container = std::make_shared<value_container>();
    container->set_message_type("query_request");

    // Serialize operation (uint8)
    std::string operation_key = "operation";
    auto operation_value = std::make_shared<uint8_value>(
        operation_key, static_cast<uint8_t>(request.operation));
    container->add(operation_value);

    // Serialize query_string (string)
    std::string query_key = "query_string";
    auto query_value = std::make_shared<string_value>(query_key, request.query_string);
    container->add(query_value);

    // Serialize parameters count (uint32)
    std::string param_count_key = "param_count";
    auto param_count_value = std::make_shared<uint32_value>(
        param_count_key, static_cast<uint32_t>(request.parameters.size()));
    container->add(param_count_value);

    // Serialize each parameter (string)
    for (size_t i = 0; i < request.parameters.size(); ++i) {
        std::string param_key = "param_" + std::to_string(i);
        auto param_value = std::make_shared<string_value>(param_key, request.parameters[i]);
        container->add(param_value);
    }

    // Serialize to bytes
    return container->serialize_array();
}

result<query_request> container_protocol_serializer::deserialize_container_query_request(
    const std::vector<uint8_t>& data) {
    try {
        // Create container from bytes
        auto container = std::make_shared<value_container>(data, false);

        // Validate message type
        if (container->message_type() != "query_request") {
            return error{error_code::invalid_argument,
                         "Invalid message type: " + container->message_type()};
        }

        query_request request;

        // Deserialize operation
        auto operation_val = container->get_value<uint8_value>("operation");
        if (operation_val) {
            request.operation = static_cast<query_operation>(operation_val->value());
        } else {
            return error{error_code::invalid_argument, "Missing operation field"};
        }

        // Deserialize query_string
        auto query_val = container->get_value<string_value>("query_string");
        if (query_val) {
            request.query_string = query_val->value();
        } else {
            return error{error_code::invalid_argument, "Missing query_string field"};
        }

        // Deserialize parameters count
        auto param_count_val = container->get_value<uint32_value>("param_count");
        if (param_count_val) {
            uint32_t count = param_count_val->value();
            request.parameters.reserve(count);

            // Deserialize each parameter
            for (uint32_t i = 0; i < count; ++i) {
                std::string param_key = "param_" + std::to_string(i);
                auto param_val = container->get_value<string_value>(param_key);
                if (param_val) {
                    request.parameters.push_back(param_val->value());
                } else {
                    return error{error_code::invalid_argument,
                                 "Missing parameter at index " + std::to_string(i)};
                }
            }
        } else {
            return error{error_code::invalid_argument, "Missing param_count field"};
        }

        return request;
    } catch (const std::exception& e) {
        return error{error_code::runtime_error,
                     std::string("Deserialization exception: ") + e.what()};
    }
}

std::string container_protocol_serializer::serialize_to_json(const query_request& request) {
    // Create container
    auto container = std::make_shared<value_container>();
    container->set_message_type("query_request");

    // Serialize fields
    std::string operation_key = "operation";
    auto operation_value = std::make_shared<uint8_value>(
        operation_key, static_cast<uint8_t>(request.operation));
    container->add(operation_value);

    std::string query_key = "query_string";
    auto query_value = std::make_shared<string_value>(query_key, request.query_string);
    container->add(query_value);

    std::string param_count_key = "param_count";
    auto param_count_value = std::make_shared<uint32_value>(
        param_count_key, static_cast<uint32_t>(request.parameters.size()));
    container->add(param_count_value);

    for (size_t i = 0; i < request.parameters.size(); ++i) {
        std::string param_key = "param_" + std::to_string(i);
        auto param_value = std::make_shared<string_value>(param_key, request.parameters[i]);
        container->add(param_value);
    }

    // Serialize to JSON
    return container->to_json();
}

} // namespace database::protocol

#endif // USE_CONTAINER_SYSTEM
