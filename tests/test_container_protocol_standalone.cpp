// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @brief Standalone test for container protocol (Phase 1 POC Validation)
 *
 * This standalone version tests only the protocol serialization/deserialization
 * without requiring full database library (which depends on network_system).
 */

#ifdef USE_CONTAINER_SYSTEM

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>

// Include only necessary protocol headers
#include <kcenon/database/protocol/database_protocol.h>
#include <kcenon/database/protocol/database_protocol_container.h>

using namespace database::protocol;

// Simple assertion macro
#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "❌ FAILED: " << message << std::endl; \
        return false; \
    }

#define TEST_SUCCESS(message) \
    std::cout << "✓ " << message << std::endl;

bool test_basic_serialization() {
    query_request request;
    request.operation = query_operation::SELECT;
    request.query_string = "SELECT * FROM users";
    request.parameters = {};

    auto serialized = container_protocol_serializer::serialize_container(request);
    TEST_ASSERT(serialized.size() > 0, "Serialized data should not be empty");

    TEST_SUCCESS("Basic serialization: " << serialized.size() << " bytes");
    return true;
}

bool test_serialization_with_parameters() {
    query_request request;
    request.operation = query_operation::SELECT;
    request.query_string = "SELECT * FROM users WHERE id = ? AND name = ?";
    request.parameters = {"123", "John Doe"};

    auto serialized = container_protocol_serializer::serialize_container(request);
    TEST_ASSERT(serialized.size() > 0, "Serialized data should not be empty");

    TEST_SUCCESS("Serialization with parameters: " << serialized.size() << " bytes");
    return true;
}

bool test_round_trip_simple() {
    query_request original;
    original.operation = query_operation::INSERT;
    original.query_string = "INSERT INTO users VALUES (?, ?)";
    original.parameters = {};

    auto serialized = container_protocol_serializer::serialize_container(original);
    auto result = container_protocol_serializer::deserialize_container_query_request(serialized);

    TEST_ASSERT(result.is_ok(), "Deserialization should succeed");

    auto recovered = result.value();
    TEST_ASSERT(recovered.operation == original.operation, "Operation should match");
    TEST_ASSERT(recovered.query_string == original.query_string, "Query string should match");
    TEST_ASSERT(recovered.parameters.size() == original.parameters.size(), "Parameter count should match");

    TEST_SUCCESS("Round-trip (simple)");
    return true;
}

bool test_round_trip_with_parameters() {
    query_request original;
    original.operation = query_operation::SELECT;
    original.query_string = "UPDATE users SET name = ? WHERE id = ?";
    original.parameters = {"Alice", "456"};

    auto serialized = container_protocol_serializer::serialize_container(original);
    auto result = container_protocol_serializer::deserialize_container_query_request(serialized);

    TEST_ASSERT(result.is_ok(), "Deserialization should succeed");

    auto recovered = result.value();
    TEST_ASSERT(recovered.operation == original.operation, "Operation should match");
    TEST_ASSERT(recovered.query_string == original.query_string, "Query string should match");
    TEST_ASSERT(recovered.parameters.size() == original.parameters.size(), "Parameter count should match");

    for (size_t i = 0; i < original.parameters.size(); ++i) {
        TEST_ASSERT(recovered.parameters[i] == original.parameters[i],
                   "Parameter " << i << " should match");
    }

    TEST_SUCCESS("Round-trip (with parameters)");
    return true;
}

bool test_special_characters() {
    query_request original;
    original.operation = query_operation::SELECT;
    original.query_string = "SELECT * FROM users WHERE name LIKE ?";
    original.parameters = {"O'Brien", "50%", "tab\tchar", "newline\nchar"};

    auto serialized = container_protocol_serializer::serialize_container(original);
    auto result = container_protocol_serializer::deserialize_container_query_request(serialized);

    TEST_ASSERT(result.is_ok(), "Deserialization with special chars should succeed");

    auto recovered = result.value();
    for (size_t i = 0; i < original.parameters.size(); ++i) {
        TEST_ASSERT(recovered.parameters[i] == original.parameters[i],
                   "Special character parameter " << i << " should match");
    }

    TEST_SUCCESS("Special characters handled correctly");
    return true;
}

bool test_many_parameters() {
    query_request original;
    original.operation = query_operation::SELECT;
    original.query_string = "BULK INSERT";

    for (int i = 0; i < 100; ++i) {
        original.parameters.push_back("param_" + std::to_string(i));
    }

    auto serialized = container_protocol_serializer::serialize_container(original);
    auto result = container_protocol_serializer::deserialize_container_query_request(serialized);

    TEST_ASSERT(result.is_ok(), "Deserialization with 100 params should succeed");

    auto recovered = result.value();
    TEST_ASSERT(recovered.parameters.size() == 100, "Should have 100 parameters");

    for (size_t i = 0; i < 100; ++i) {
        TEST_ASSERT(recovered.parameters[i] == "param_" + std::to_string(i),
                   "Parameter " << i << " should match");
    }

    TEST_SUCCESS("100 parameters handled correctly");
    return true;
}

bool test_json_serialization() {
    query_request request;
    request.operation = query_operation::SELECT;
    request.query_string = "SELECT * FROM test";
    request.parameters = {"value1", "value2"};

    auto json = container_protocol_serializer::serialize_to_json(request);

    TEST_ASSERT(json.size() > 0, "JSON should not be empty");
    TEST_ASSERT(json.find("query_request") != std::string::npos, "JSON should contain message type");
    TEST_ASSERT(json.find("query_string") != std::string::npos, "JSON should contain query_string");

    std::cout << "JSON output sample:\n" << json.substr(0, 200) << "..." << std::endl;
    TEST_SUCCESS("JSON serialization");
    return true;
}

bool test_invalid_data() {
    std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03};

    auto result = container_protocol_serializer::deserialize_container_query_request(invalid_data);

    TEST_ASSERT(result.is_err(), "Invalid data should return error");
    TEST_SUCCESS("Invalid data correctly rejected");
    return true;
}

void performance_test() {
    std::cout << "\n=== Performance Test ===" << std::endl;

    query_request request;
    request.operation = query_operation::SELECT;
    request.query_string = "SELECT * FROM performance_test WHERE id = ?";
    request.parameters = {"test_value"};

    const int iterations = 10000;

    // Serialization performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto serialized = container_protocol_serializer::serialize_container(request);
        volatile size_t size = serialized.size();
        (void)size;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (iterations * 1000000.0) / duration.count();
    double us_per_op = duration.count() / static_cast<double>(iterations);

    std::cout << "Container serialization:" << std::endl;
    std::cout << "  " << iterations << " iterations in " << duration.count() << " μs" << std::endl;
    std::cout << "  " << static_cast<int>(ops_per_sec) << " ops/sec" << std::endl;
    std::cout << "  " << us_per_op << " μs/op" << std::endl;

    // Round-trip performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto serialized = container_protocol_serializer::serialize_container(request);
        auto result = container_protocol_serializer::deserialize_container_query_request(serialized);
        volatile bool ok = result.is_ok();
        (void)ok;
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    ops_per_sec = (iterations * 1000000.0) / duration.count();
    us_per_op = duration.count() / static_cast<double>(iterations);

    std::cout << "\nRound-trip (serialize + deserialize):" << std::endl;
    std::cout << "  " << iterations << " iterations in " << duration.count() << " μs" << std::endl;
    std::cout << "  " << static_cast<int>(ops_per_sec) << " ops/sec" << std::endl;
    std::cout << "  " << us_per_op << " μs/op" << std::endl;
}

int main() {
    std::cout << "=== Container Protocol Phase 1 POC Validation ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    #define RUN_TEST(test_func) \
        do { \
            total++; \
            if (test_func()) { \
                passed++; \
            } else { \
                std::cerr << "Test failed: " #test_func << std::endl; \
            } \
        } while(0)

    RUN_TEST(test_basic_serialization);
    RUN_TEST(test_serialization_with_parameters);
    RUN_TEST(test_round_trip_simple);
    RUN_TEST(test_round_trip_with_parameters);
    RUN_TEST(test_special_characters);
    RUN_TEST(test_many_parameters);
    RUN_TEST(test_json_serialization);
    RUN_TEST(test_invalid_data);

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;

    if (passed == total) {
        std::cout << "✅ All tests passed!" << std::endl;
        performance_test();
        return 0;
    } else {
        std::cout << "❌ Some tests failed" << std::endl;
        return 1;
    }
}

#else
int main() {
    std::cerr << "Container system not enabled. Compile with -DUSE_CONTAINER_SYSTEM=ON" << std::endl;
    return 1;
}
#endif // USE_CONTAINER_SYSTEM
