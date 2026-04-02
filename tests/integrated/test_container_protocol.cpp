// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#ifdef USE_CONTAINER_SYSTEM

#include <gtest/gtest.h>
#include <protocol/database_protocol.h>
#include <protocol/database_protocol_container.h>
#include <string>
#include <vector>

using namespace database::protocol;

/**
 * @brief Test suite for container-based protocol serialization
 *
 * Validates Phase 1 POC implementation:
 * - Serialization correctness
 * - Deserialization correctness
 * - Round-trip integrity
 * - Error handling
 * - JSON format support
 */
class ContainerProtocolTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup common test data
  }

  void TearDown() override {
    // Cleanup
  }
};

/**
 * Test: Basic serialization without parameters
 */
TEST_F(ContainerProtocolTest, SerializeBasicQueryRequest) {
  query_request request;
  request.operation = query_operation::SELECT;
  request.query_string = "SELECT * FROM users";
  request.parameters = {};

  // Serialize using container system
  auto serialized = container_protocol_serializer::serialize_container(request);

  // Verify serialization produces data
  EXPECT_GT(serialized.size(), 0u);

  std::cout << "Serialized " << request.query_string << " to "
            << serialized.size() << " bytes" << std::endl;
}

/**
 * Test: Serialization with parameters
 */
TEST_F(ContainerProtocolTest, SerializeQueryRequestWithParameters) {
  query_request request;
  request.operation = query_operation::SELECT;
  request.query_string = "SELECT * FROM users WHERE id = ? AND name = ?";
  request.parameters = {"123", "John Doe"};

  auto serialized = container_protocol_serializer::serialize_container(request);

  EXPECT_GT(serialized.size(), 0u);

  std::cout << "Serialized query with " << request.parameters.size()
            << " parameters to " << serialized.size() << " bytes" << std::endl;
}

/**
 * Test: Deserialization without parameters
 */
TEST_F(ContainerProtocolTest, DeserializeBasicQueryRequest) {
  // Create request
  query_request original;
  original.operation = query_operation::INSERT;
  original.query_string = "INSERT INTO users VALUES (?, ?)";
  original.parameters = {};

  // Serialize
  auto serialized =
      container_protocol_serializer::serialize_container(original);

  // Deserialize
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  // Verify success
  ASSERT_TRUE(result.is_ok());

  auto deserialized = result.unwrap();
  EXPECT_EQ(deserialized.operation, original.operation);
  EXPECT_EQ(deserialized.query_string, original.query_string);
  EXPECT_EQ(deserialized.parameters.size(), original.parameters.size());

  std::cout << "Successfully deserialized: " << deserialized.query_string
            << std::endl;
}

/**
 * Test: Deserialization with parameters
 */
TEST_F(ContainerProtocolTest, DeserializeQueryRequestWithParameters) {
  query_request original;
  original.operation = query_operation::UPDATE;
  original.query_string = "UPDATE users SET name = ? WHERE id = ?";
  original.parameters = {"Alice", "456"};

  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());

  auto deserialized = result.unwrap();
  EXPECT_EQ(deserialized.operation, original.operation);
  EXPECT_EQ(deserialized.query_string, original.query_string);
  ASSERT_EQ(deserialized.parameters.size(), original.parameters.size());

  for (size_t i = 0; i < original.parameters.size(); ++i) {
    EXPECT_EQ(deserialized.parameters[i], original.parameters[i]);
  }

  std::cout << "Successfully deserialized query with "
            << deserialized.parameters.size() << " parameters" << std::endl;
}

/**
 * Test: Round-trip integrity - simple case
 */
TEST_F(ContainerProtocolTest, RoundTripSimple) {
  query_request original;
  original.operation = query_operation::SELECT;
  original.query_string = "SELECT COUNT(*) FROM orders";
  original.parameters = {};

  // Round-trip
  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());
  auto recovered = result.unwrap();

  // Verify exact match
  EXPECT_EQ(recovered.operation, original.operation);
  EXPECT_EQ(recovered.query_string, original.query_string);
  EXPECT_EQ(recovered.parameters.size(), original.parameters.size());

  std::cout << "✓ Round-trip integrity verified (simple)" << std::endl;
}

/**
 * Test: Round-trip integrity - complex case with many parameters
 */
TEST_F(ContainerProtocolTest, RoundTripComplex) {
  query_request original;
  original.operation = query_operation::INSERT;
  original.query_string = "INSERT INTO products (id, name, price, category, "
                          "stock) VALUES (?, ?, ?, ?, ?)";
  original.parameters = {"1001", "Laptop", "999.99", "Electronics", "42"};

  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());
  auto recovered = result.unwrap();

  EXPECT_EQ(recovered.operation, original.operation);
  EXPECT_EQ(recovered.query_string, original.query_string);
  ASSERT_EQ(recovered.parameters.size(), original.parameters.size());

  for (size_t i = 0; i < original.parameters.size(); ++i) {
    EXPECT_EQ(recovered.parameters[i], original.parameters[i])
        << "Parameter mismatch at index " << i;
  }

  std::cout << "✓ Round-trip integrity verified (complex, "
            << original.parameters.size() << " parameters)" << std::endl;
}

/**
 * Test: Round-trip with special characters
 */
TEST_F(ContainerProtocolTest, RoundTripSpecialCharacters) {
  query_request original;
  original.operation = query_operation::SELECT;
  original.query_string = "SELECT * FROM users WHERE name LIKE ?";
  original.parameters = {"O'Brien", "50%", "tab\tchar", "quote\"char"};

  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());
  auto recovered = result.unwrap();

  ASSERT_EQ(recovered.parameters.size(), original.parameters.size());
  for (size_t i = 0; i < original.parameters.size(); ++i) {
    EXPECT_EQ(recovered.parameters[i], original.parameters[i])
        << "Special character parameter mismatch at index " << i;
  }

  std::cout << "✓ Special characters handled correctly" << std::endl;
}

/**
 * Test: Empty query string
 */
TEST_F(ContainerProtocolTest, EmptyQueryString) {
  query_request original;
  original.operation = query_operation::SELECT;
  original.query_string = "";
  original.parameters = {};

  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());
  auto recovered = result.unwrap();

  EXPECT_EQ(recovered.query_string, "");
  std::cout << "✓ Empty query string handled" << std::endl;
}

/**
 * Test: Large number of parameters
 */
TEST_F(ContainerProtocolTest, ManyParameters) {
  query_request original;
  original.operation = query_operation::SELECT;
  original.query_string = "BULK INSERT";

  // Create 100 parameters
  for (int i = 0; i < 100; ++i) {
    original.parameters.push_back("param_" + std::to_string(i));
  }

  auto serialized =
      container_protocol_serializer::serialize_container(original);
  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          serialized);

  ASSERT_TRUE(result.is_ok());
  auto recovered = result.unwrap();

  EXPECT_EQ(recovered.parameters.size(), 100u);
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(recovered.parameters[i], "param_" + std::to_string(i));
  }

  std::cout << "✓ 100 parameters handled correctly" << std::endl;
}

/**
 * Test: JSON serialization
 */
TEST_F(ContainerProtocolTest, JSONSerialization) {
  query_request request;
  request.operation = query_operation::SELECT;
  request.query_string = "SELECT * FROM test";
  request.parameters = {"value1", "value2"};

  auto json = container_protocol_serializer::serialize_to_json(request);

  // Verify JSON is not empty and contains expected elements
  EXPECT_GT(json.size(), 0u);
  EXPECT_NE(json.find("query_request"), std::string::npos);
  EXPECT_NE(json.find("query_string"), std::string::npos);

  std::cout << "JSON output:\n" << json << std::endl;
}

/**
 * Test: Invalid data deserialization
 */
TEST_F(ContainerProtocolTest, DeserializeInvalidData) {
  std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03};

  auto result =
      container_protocol_serializer::deserialize_container_query_request(
          invalid_data);

  // Should return error
  EXPECT_TRUE(result.is_err());

  if (result.is_err()) {
    std::cout << "✓ Invalid data correctly rejected: " << result.error().message
              << std::endl;
  }
}

/**
 * Test: All query operations
 */
TEST_F(ContainerProtocolTest, AllQueryOperations) {
  std::vector<query_operation> operations = {
      query_operation::SELECT, query_operation::INSERT, query_operation::UPDATE,
      query_operation::DELETE};

  for (auto op : operations) {
    query_request original;
    original.operation = op;
    original.query_string = "TEST QUERY";
    original.parameters = {};

    auto serialized =
        container_protocol_serializer::serialize_container(original);
    auto result =
        container_protocol_serializer::deserialize_container_query_request(
            serialized);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap().operation, op);
  }

  std::cout << "✓ All query operations verified" << std::endl;
}

/**
 * Performance indicator test - measure serialization speed
 */
TEST_F(ContainerProtocolTest, PerformanceIndicator) {
  query_request request;
  request.operation = query_operation::SELECT;
  request.query_string = "SELECT * FROM performance_test WHERE id = ?";
  request.parameters = {"test_value"};

  const int iterations = 10000;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < iterations; ++i) {
    auto serialized =
        container_protocol_serializer::serialize_container(request);
    // Don't optimize away
    volatile size_t size = serialized.size();
    (void)size;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double ops_per_sec = (iterations * 1000000.0) / duration.count();
  double us_per_op = duration.count() / static_cast<double>(iterations);

  std::cout << "Container serialization performance:" << std::endl;
  std::cout << "  " << iterations << " iterations in " << duration.count()
            << " μs" << std::endl;
  std::cout << "  " << static_cast<int>(ops_per_sec) << " ops/sec" << std::endl;
  std::cout << "  " << us_per_op << " μs/op" << std::endl;

  // Performance expectation: should be > 100K ops/sec
#ifdef NDEBUG
  EXPECT_GT(ops_per_sec, 100000.0) << "Performance below expectations";
#else
  EXPECT_GT(ops_per_sec, 10000.0)
      << "Performance below expectations (Debug build)";
#endif
}

#endif // USE_CONTAINER_SYSTEM
