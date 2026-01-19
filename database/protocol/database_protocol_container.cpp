/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#ifdef USE_CONTAINER_SYSTEM

#include "database_protocol_container.h"
#include <core/container.h>
#include <core/value_types.h>

namespace database::protocol {

using namespace container_module;

std::vector<uint8_t> container_protocol_serializer::serialize_container(
    const query_request& request) {
    // Create container
    auto container = std::make_shared<value_container>();
    container->set_message_type("query_request");

    // Serialize operation (as int since uint8 is promoted to int in variant)
    container->add_value("operation", static_cast<int>(request.operation));

    // Serialize query_string
    container->add_value("query_string", std::string(request.query_string));

    // Serialize parameters count
    container->add_value("param_count", static_cast<unsigned int>(request.parameters.size()));

    // Serialize each parameter
    for (size_t i = 0; i < request.parameters.size(); ++i) {
        std::string param_key = "param_" + std::to_string(i);
        container->add_value(param_key, std::string(request.parameters[i]));
    }

    // Serialize to bytes
    return container->serialize_array();
}

kcenon::common::Result<query_request> container_protocol_serializer::deserialize_container_query_request(
    const std::vector<uint8_t>& data) {
    try {
        // Create container from bytes
        auto container = std::make_shared<value_container>(data, false);

        // Validate message type
        if (container->message_type() != "query_request") {
            return kcenon::common::error_info{
                static_cast<int>(error_code::invalid_argument),
                "Invalid message type: " + container->message_type(),
                "database_protocol"};
        }

        query_request request;

        // Deserialize operation
        auto operation_val = container->get_value("operation");
        if (operation_val.has_value()) {
            if (std::holds_alternative<int>(operation_val->data)) {
                request.operation = static_cast<query_operation>(
                    std::get<int>(operation_val->data));
            } else {
                return kcenon::common::error_info{
                    static_cast<int>(error_code::invalid_argument),
                    "Invalid operation field type",
                    "database_protocol"};
            }
        } else {
            return kcenon::common::error_info{
                static_cast<int>(error_code::invalid_argument),
                "Missing operation field",
                "database_protocol"};
        }

        // Deserialize query_string
        auto query_val = container->get_value("query_string");
        if (query_val.has_value()) {
            if (std::holds_alternative<std::string>(query_val->data)) {
                request.query_string = std::get<std::string>(query_val->data);
            } else {
                return kcenon::common::error_info{
                    static_cast<int>(error_code::invalid_argument),
                    "Invalid query_string field type",
                    "database_protocol"};
            }
        } else {
            return kcenon::common::error_info{
                static_cast<int>(error_code::invalid_argument),
                "Missing query_string field",
                "database_protocol"};
        }

        // Deserialize parameters count
        auto param_count_val = container->get_value("param_count");
        if (param_count_val.has_value()) {
            if (std::holds_alternative<unsigned int>(param_count_val->data)) {
                uint32_t count = std::get<unsigned int>(param_count_val->data);
                request.parameters.reserve(count);

                // Deserialize each parameter
                for (uint32_t i = 0; i < count; ++i) {
                    std::string param_key = "param_" + std::to_string(i);
                    auto param_val = container->get_value(param_key);
                    if (param_val.has_value()) {
                        if (std::holds_alternative<std::string>(param_val->data)) {
                            request.parameters.push_back(std::get<std::string>(param_val->data));
                        } else {
                            return kcenon::common::error_info{
                                static_cast<int>(error_code::invalid_argument),
                                "Invalid parameter type at index " + std::to_string(i),
                                "database_protocol"};
                        }
                    } else {
                        return kcenon::common::error_info{
                            static_cast<int>(error_code::invalid_argument),
                            "Missing parameter at index " + std::to_string(i),
                            "database_protocol"};
                    }
                }
            } else {
                return kcenon::common::error_info{
                    static_cast<int>(error_code::invalid_argument),
                    "Invalid param_count field type",
                    "database_protocol"};
            }
        } else {
            return kcenon::common::error_info{
                static_cast<int>(error_code::invalid_argument),
                "Missing param_count field",
                "database_protocol"};
        }

        return request;
    } catch (const std::exception& e) {
        return kcenon::common::error_info{
            static_cast<int>(error_code::invalid_argument),
            std::string("Deserialization exception: ") + e.what(),
            "database_protocol"};
    }
}

std::string container_protocol_serializer::serialize_to_json(const query_request& request) {
    // Create container
    auto container = std::make_shared<value_container>();
    container->set_message_type("query_request");

    // Serialize fields using template add_value for automatic type deduction
    container->add_value("operation", static_cast<int>(request.operation));
    container->add_value("query_string", std::string(request.query_string));
    container->add_value("param_count", static_cast<unsigned int>(request.parameters.size()));

    for (size_t i = 0; i < request.parameters.size(); ++i) {
        std::string param_key = "param_" + std::to_string(i);
        container->add_value(param_key, std::string(request.parameters[i]));
    }

    // Serialize to JSON
    return container->to_json();
}

} // namespace database::protocol

#endif // USE_CONTAINER_SYSTEM
