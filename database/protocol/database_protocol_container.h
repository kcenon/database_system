/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#ifdef USE_CONTAINER_SYSTEM

#include "database_protocol.h"
#include <core/container.h>
#include <memory>

namespace database::protocol {

/**
 * @class container_protocol_serializer
 * @brief Container-based serialization for protocol messages (Phase 1 POC)
 *
 * This class demonstrates the container_system integration for database protocol
 * serialization. Phase 1 focuses on query_request as a proof of concept.
 *
 * Benefits over manual serialization:
 * - Type safety with compile-time checks
 * - SIMD optimization support
 * - Cleaner, more maintainable code
 * - Multiple format support (binary, JSON, XML)
 */
class container_protocol_serializer {
public:
    /**
     * @brief Serialize query_request using container_system
     * @param request Query request to serialize
     * @return Serialized bytes
     */
    static std::vector<uint8_t> serialize_container(const query_request& request);

    /**
     * @brief Deserialize query_request using container_system
     * @param data Serialized bytes
     * @return Deserialized request or error
     */
    static kcenon::common::Result<query_request> deserialize_container_query_request(
        const std::vector<uint8_t>& data);

    /**
     * @brief Serialize query_request to JSON for debugging
     * @param request Query request to serialize
     * @return JSON string
     */
    static std::string serialize_to_json(const query_request& request);
};

} // namespace database::protocol

#endif // USE_CONTAINER_SYSTEM
