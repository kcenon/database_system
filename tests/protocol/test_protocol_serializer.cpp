// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Unit tests for protocol_serializer binary round-trip serialization.
 * Part of #367, sub-issue #378.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include <kcenon/database/protocol/database_protocol.h>

using namespace database::protocol;

//=============================================================================
// message_header Tests
//=============================================================================

class MessageHeaderTest : public ::testing::Test {};

TEST_F(MessageHeaderTest, DefaultMagicAndVersion) {
	message_header header;
	EXPECT_EQ(header.magic, message_header::MAGIC);
	EXPECT_EQ(header.version, message_header::PROTOCOL_VERSION);
	EXPECT_TRUE(header.is_valid());
}

TEST_F(MessageHeaderTest, InvalidMagic) {
	message_header header;
	header.magic = 0xDEADBEEF;
	EXPECT_FALSE(header.is_valid());
}

TEST_F(MessageHeaderTest, InvalidVersion) {
	message_header header;
	header.version = 99;
	EXPECT_FALSE(header.is_valid());
}

TEST_F(MessageHeaderTest, SerializeDeserializeRoundTrip) {
	message_header original;
	original.type = message_type::QUERY_REQUEST;
	original.request_id = 12345;
	original.payload_size = 100;

	auto bytes = protocol_serializer::serialize_header(original);
	EXPECT_EQ(bytes.size(), 20u);

	auto result = protocol_serializer::deserialize_header(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.magic, original.magic);
	EXPECT_EQ(recovered.version, original.version);
	EXPECT_EQ(recovered.type, original.type);
	EXPECT_EQ(recovered.request_id, original.request_id);
	EXPECT_EQ(recovered.payload_size, original.payload_size);
}

TEST_F(MessageHeaderTest, DeserializeHeaderTooSmall) {
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = protocol_serializer::deserialize_header(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(MessageHeaderTest, DeserializeInvalidMagicReturnsError) {
	// Build a 20-byte buffer with wrong magic
	std::vector<uint8_t> data(20, 0);
	// Write garbage magic (little-endian)
	data[0] = 0xEF; data[1] = 0xBE; data[2] = 0xAD; data[3] = 0xDE;
	// Write valid version (1, little-endian)
	data[4] = 0x01; data[5] = 0x00;

	auto result = protocol_serializer::deserialize_header(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(MessageHeaderTest, AllMessageTypes) {
	const message_type types[] = {
		message_type::CONNECT_REQUEST, message_type::CONNECT_RESPONSE,
		message_type::DISCONNECT, message_type::PING, message_type::PONG,
		message_type::QUERY_REQUEST, message_type::QUERY_RESPONSE,
		message_type::BEGIN_TRANSACTION, message_type::COMMIT_TRANSACTION,
		message_type::ROLLBACK_TRANSACTION, message_type::TRANSACTION_RESPONSE,
		message_type::PREPARE_STATEMENT, message_type::EXECUTE_PREPARED,
		message_type::CLOSE_PREPARED, message_type::ERROR_RESPONSE
	};

	for (auto type : types) {
		message_header header;
		header.type = type;
		header.request_id = 42;
		header.payload_size = 0;

		auto bytes = protocol_serializer::serialize_header(header);
		auto result = protocol_serializer::deserialize_header(bytes);
		ASSERT_TRUE(result.is_ok()) << "Failed for message_type " << static_cast<uint16_t>(type);
		EXPECT_EQ(result.value().type, type);
	}
}

TEST_F(MessageHeaderTest, LargeRequestIdAndPayload) {
	message_header header;
	header.type = message_type::QUERY_REQUEST;
	header.request_id = UINT64_MAX;
	header.payload_size = UINT32_MAX;

	auto bytes = protocol_serializer::serialize_header(header);
	auto result = protocol_serializer::deserialize_header(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().request_id, UINT64_MAX);
	EXPECT_EQ(result.value().payload_size, UINT32_MAX);
}

//=============================================================================
// connect_request / connect_response Tests
//=============================================================================

class ConnectProtocolTest : public ::testing::Test {};

TEST_F(ConnectProtocolTest, ConnectRequestRoundTrip) {
	connect_request original;
	original.database_type = "postgresql";
	original.connection_string = "host=localhost port=5432 dbname=testdb";
	original.options = {{"timeout", "30"}, {"ssl", "true"}};

	auto bytes = protocol_serializer::serialize(original);
	EXPECT_FALSE(bytes.empty());

	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.database_type, original.database_type);
	EXPECT_EQ(recovered.connection_string, original.connection_string);
	EXPECT_EQ(recovered.options.size(), original.options.size());
	EXPECT_EQ(recovered.options.at("timeout"), "30");
	EXPECT_EQ(recovered.options.at("ssl"), "true");
}

TEST_F(ConnectProtocolTest, ConnectRequestEmptyOptions) {
	connect_request original;
	original.database_type = "sqlite";
	original.connection_string = "file:test.db";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.database_type, "sqlite");
	EXPECT_EQ(recovered.connection_string, "file:test.db");
	EXPECT_TRUE(recovered.options.empty());
}

TEST_F(ConnectProtocolTest, ConnectRequestTooSmall) {
	std::vector<uint8_t> data = {0x01, 0x02};
	auto result = protocol_serializer::deserialize_connect_request(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectProtocolTest, ConnectResponseSuccessRoundTrip) {
	connect_response original;
	original.success = true;
	original.session_id = "sess-abc-123-def-456";
	original.error_message = "";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_TRUE(recovered.success);
	EXPECT_EQ(recovered.session_id, "sess-abc-123-def-456");
	EXPECT_TRUE(recovered.error_message.empty());
}

TEST_F(ConnectProtocolTest, ConnectResponseFailureRoundTrip) {
	connect_response original;
	original.success = false;
	original.session_id = "";
	original.error_message = "Authentication failed: invalid credentials";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_FALSE(recovered.success);
	EXPECT_TRUE(recovered.session_id.empty());
	EXPECT_EQ(recovered.error_message, "Authentication failed: invalid credentials");
}

TEST_F(ConnectProtocolTest, ConnectResponseEmptyData) {
	std::vector<uint8_t> data;
	auto result = protocol_serializer::deserialize_connect_response(data);
	EXPECT_TRUE(result.is_err());
}

//=============================================================================
// query_request / query_response Tests
//=============================================================================

class QueryProtocolTest : public ::testing::Test {};

TEST_F(QueryProtocolTest, QueryRequestSelectRoundTrip) {
	query_request original;
	original.operation = query_operation::SELECT;
	original.query_string = "SELECT * FROM users WHERE id = ?";
	original.parameters = {"42"};

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.operation, query_operation::SELECT);
	EXPECT_EQ(recovered.query_string, original.query_string);
	ASSERT_EQ(recovered.parameters.size(), 1u);
	EXPECT_EQ(recovered.parameters[0], "42");
}

TEST_F(QueryProtocolTest, QueryRequestInsertWithMultipleParams) {
	query_request original;
	original.operation = query_operation::INSERT;
	original.query_string = "INSERT INTO users (name, email, age) VALUES (?, ?, ?)";
	original.parameters = {"Alice", "alice@example.com", "30"};

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.operation, query_operation::INSERT);
	ASSERT_EQ(recovered.parameters.size(), 3u);
	EXPECT_EQ(recovered.parameters[0], "Alice");
	EXPECT_EQ(recovered.parameters[1], "alice@example.com");
	EXPECT_EQ(recovered.parameters[2], "30");
}

TEST_F(QueryProtocolTest, QueryRequestNoParameters) {
	query_request original;
	original.operation = query_operation::CREATE;
	original.query_string = "CREATE TABLE test (id INTEGER PRIMARY KEY)";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.operation, query_operation::CREATE);
	EXPECT_TRUE(recovered.parameters.empty());
}

TEST_F(QueryProtocolTest, QueryRequestAllOperationTypes) {
	const query_operation ops[] = {
		query_operation::SELECT, query_operation::INSERT,
		query_operation::UPDATE, query_operation::DELETE,
		query_operation::CREATE, query_operation::ALTER,
		query_operation::DROP, query_operation::OTHER
	};

	for (auto op : ops) {
		query_request original;
		original.operation = op;
		original.query_string = "test query";

		auto bytes = protocol_serializer::serialize(original);
		auto result = protocol_serializer::deserialize_query_request(bytes);
		ASSERT_TRUE(result.is_ok()) << "Failed for operation " << static_cast<int>(op);
		EXPECT_EQ(result.value().operation, op);
	}
}

TEST_F(QueryProtocolTest, QueryRequestSpecialCharacters) {
	query_request original;
	original.operation = query_operation::SELECT;
	original.query_string = "SELECT * FROM users WHERE name LIKE ?";
	original.parameters = {"O'Brien", "50%", "tab\tchar", "newline\nchar"};

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	ASSERT_EQ(recovered.parameters.size(), 4u);
	EXPECT_EQ(recovered.parameters[0], "O'Brien");
	EXPECT_EQ(recovered.parameters[1], "50%");
	EXPECT_EQ(recovered.parameters[2], "tab\tchar");
	EXPECT_EQ(recovered.parameters[3], "newline\nchar");
}

TEST_F(QueryProtocolTest, QueryRequestEmptyData) {
	std::vector<uint8_t> data;
	auto result = protocol_serializer::deserialize_query_request(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QueryProtocolTest, QueryResponseSuccessWithRows) {
	query_response original;
	original.success = true;
	original.affected_rows = 0;
	original.last_insert_id = 0;
	original.error_code = 0;
	original.column_names = {"id", "name", "email"};
	original.rows = {
		{{"id", "1"}, {"name", "Alice"}, {"email", "alice@test.com"}},
		{{"id", "2"}, {"name", "Bob"}, {"email", "bob@test.com"}}
	};

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_TRUE(recovered.success);
	EXPECT_EQ(recovered.affected_rows, 0u);
	ASSERT_EQ(recovered.column_names.size(), 3u);
	EXPECT_EQ(recovered.column_names[0], "id");
	EXPECT_EQ(recovered.column_names[1], "name");
	EXPECT_EQ(recovered.column_names[2], "email");
	ASSERT_EQ(recovered.rows.size(), 2u);
	EXPECT_EQ(recovered.rows[0].at("name"), "Alice");
	EXPECT_EQ(recovered.rows[1].at("email"), "bob@test.com");
}

TEST_F(QueryProtocolTest, QueryResponseInsertResult) {
	query_response original;
	original.success = true;
	original.affected_rows = 1;
	original.last_insert_id = 42;
	original.error_code = 0;

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_TRUE(recovered.success);
	EXPECT_EQ(recovered.affected_rows, 1u);
	EXPECT_EQ(recovered.last_insert_id, 42u);
	EXPECT_TRUE(recovered.column_names.empty());
	EXPECT_TRUE(recovered.rows.empty());
}

TEST_F(QueryProtocolTest, QueryResponseFailure) {
	query_response original;
	original.success = false;
	original.error_code = 1045;
	original.error_message = "Access denied for user 'root'@'localhost'";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_FALSE(recovered.success);
	EXPECT_EQ(recovered.error_code, 1045);
	EXPECT_EQ(recovered.error_message, "Access denied for user 'root'@'localhost'");
}

TEST_F(QueryProtocolTest, QueryResponseEmptyData) {
	std::vector<uint8_t> data;
	auto result = protocol_serializer::deserialize_query_response(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QueryProtocolTest, QueryResponseLargePayload) {
	query_response original;
	original.success = true;
	original.column_names = {"id", "data"};

	for (int i = 0; i < 100; ++i) {
		original.rows.push_back({
			{"id", std::to_string(i)},
			{"data", std::string(256, 'x')}
		});
	}

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	ASSERT_EQ(recovered.rows.size(), 100u);
	EXPECT_EQ(recovered.rows[50].at("id"), "50");
	EXPECT_EQ(recovered.rows[50].at("data").size(), 256u);
}

//=============================================================================
// transaction_request / transaction_response Tests
//=============================================================================

class TransactionProtocolTest : public ::testing::Test {};

TEST_F(TransactionProtocolTest, BeginTransactionRoundTrip) {
	transaction_request original;
	original.operation = message_type::BEGIN_TRANSACTION;

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_transaction_request(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().operation, message_type::BEGIN_TRANSACTION);
}

TEST_F(TransactionProtocolTest, CommitTransactionRoundTrip) {
	transaction_request original;
	original.operation = message_type::COMMIT_TRANSACTION;

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_transaction_request(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().operation, message_type::COMMIT_TRANSACTION);
}

TEST_F(TransactionProtocolTest, RollbackTransactionRoundTrip) {
	transaction_request original;
	original.operation = message_type::ROLLBACK_TRANSACTION;

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_transaction_request(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().operation, message_type::ROLLBACK_TRANSACTION);
}

TEST_F(TransactionProtocolTest, TransactionRequestTooSmall) {
	std::vector<uint8_t> data = {0x01};
	auto result = protocol_serializer::deserialize_transaction_request(data);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransactionProtocolTest, TransactionResponseSuccessRoundTrip) {
	transaction_response original;
	original.success = true;
	original.error_message = "";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_transaction_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_TRUE(recovered.success);
	EXPECT_TRUE(recovered.error_message.empty());
}

TEST_F(TransactionProtocolTest, TransactionResponseFailureRoundTrip) {
	transaction_response original;
	original.success = false;
	original.error_message = "Deadlock detected";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_transaction_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_FALSE(recovered.success);
	EXPECT_EQ(recovered.error_message, "Deadlock detected");
}

TEST_F(TransactionProtocolTest, TransactionResponseEmptyData) {
	std::vector<uint8_t> data;
	auto result = protocol_serializer::deserialize_transaction_response(data);
	EXPECT_TRUE(result.is_err());
}

//=============================================================================
// error_response Tests
//=============================================================================

class ErrorProtocolTest : public ::testing::Test {};

TEST_F(ErrorProtocolTest, ErrorResponseRoundTrip) {
	error_response original;
	original.error_code = 1001;
	original.error_message = "Table not found";
	original.error_context = "SELECT * FROM nonexistent_table";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_error_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.error_code, 1001);
	EXPECT_EQ(recovered.error_message, "Table not found");
	EXPECT_EQ(recovered.error_context, "SELECT * FROM nonexistent_table");
}

TEST_F(ErrorProtocolTest, ErrorResponseNegativeCode) {
	error_response original;
	original.error_code = -1;
	original.error_message = "Internal server error";
	original.error_context = "";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_error_response(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.error_code, -1);
	EXPECT_EQ(recovered.error_message, "Internal server error");
	EXPECT_TRUE(recovered.error_context.empty());
}

TEST_F(ErrorProtocolTest, ErrorResponseTooSmall) {
	std::vector<uint8_t> data = {0x01, 0x02};
	auto result = protocol_serializer::deserialize_error_response(data);
	EXPECT_TRUE(result.is_err());
}

//=============================================================================
// Cross-cutting serialization concerns
//=============================================================================

class SerializerEdgeCaseTest : public ::testing::Test {};

TEST_F(SerializerEdgeCaseTest, EmptyStringFields) {
	connect_request original;
	original.database_type = "";
	original.connection_string = "";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_TRUE(recovered.database_type.empty());
	EXPECT_TRUE(recovered.connection_string.empty());
}

TEST_F(SerializerEdgeCaseTest, LongStringPreserved) {
	connect_request original;
	original.database_type = "postgresql";
	original.connection_string = std::string(4096, 'A');

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().connection_string.size(), 4096u);
}

TEST_F(SerializerEdgeCaseTest, UnicodeStringPreserved) {
	connect_request original;
	original.database_type = "postgresql";
	original.connection_string = "dbname=\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95";

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().connection_string, original.connection_string);
}

TEST_F(SerializerEdgeCaseTest, ManyOptionsMapPreserved) {
	connect_request original;
	original.database_type = "postgresql";
	original.connection_string = "localhost";

	for (int i = 0; i < 50; ++i) {
		original.options["key_" + std::to_string(i)] = "val_" + std::to_string(i);
	}

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_connect_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	EXPECT_EQ(recovered.options.size(), 50u);
	EXPECT_EQ(recovered.options.at("key_25"), "val_25");
}

TEST_F(SerializerEdgeCaseTest, QueryRequestManyParameters) {
	query_request original;
	original.operation = query_operation::SELECT;
	original.query_string = "BULK QUERY";

	for (int i = 0; i < 100; ++i) {
		original.parameters.push_back("param_" + std::to_string(i));
	}

	auto bytes = protocol_serializer::serialize(original);
	auto result = protocol_serializer::deserialize_query_request(bytes);
	ASSERT_TRUE(result.is_ok());

	auto recovered = result.value();
	ASSERT_EQ(recovered.parameters.size(), 100u);
	EXPECT_EQ(recovered.parameters[0], "param_0");
	EXPECT_EQ(recovered.parameters[99], "param_99");
}

TEST_F(SerializerEdgeCaseTest, HeaderZeroRequestId) {
	message_header header;
	header.type = message_type::PING;
	header.request_id = 0;
	header.payload_size = 0;

	auto bytes = protocol_serializer::serialize_header(header);
	auto result = protocol_serializer::deserialize_header(bytes);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().request_id, 0u);
	EXPECT_EQ(result.value().payload_size, 0u);
}
