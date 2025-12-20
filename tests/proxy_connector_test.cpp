/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Unit tests for Phase 4.1 - ProxyMode support
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>

#include "database/core/database_context.h"
#include "database/database_manager.h"
#include "database/database_types.h"
#include "database/proxy/proxy_config.h"
#include "database/proxy/proxy_connector.h"

using namespace database;
using namespace database::proxy;

// Test fixture for proxy connector tests
class ProxyConnectorTest : public ::testing::Test {
protected:
	std::shared_ptr<database_context> context_;
	std::shared_ptr<database_manager> db_mgr_;

	void SetUp() override {
		context_ = std::make_shared<database_context>();
		db_mgr_ = std::make_shared<database_manager>(context_);
	}

	void TearDown() override {
		if (db_mgr_) {
			db_mgr_->disconnect();
		}
	}

	proxy_connection_config create_valid_config() {
		proxy_connection_config config;
		config.server_host = "localhost";
		config.server_port = 9432;
		config.auth_token = "test-token";
		config.connection_timeout = std::chrono::milliseconds{5000};
		config.query_timeout = std::chrono::milliseconds{30000};
		return config;
	}
};

// connection_mode enum tests
TEST_F(ProxyConnectorTest, ConnectionModeEnumValues) {
	// Test enum values
	EXPECT_EQ(static_cast<uint8_t>(connection_mode::direct), 0);
	EXPECT_EQ(static_cast<uint8_t>(connection_mode::proxy), 1);
}

TEST_F(ProxyConnectorTest, ConnectionModeToString) {
	// Test to_string conversion
	EXPECT_STREQ(to_string(connection_mode::direct), "direct");
	EXPECT_STREQ(to_string(connection_mode::proxy), "proxy");
}

// proxy_connection_config tests
TEST_F(ProxyConnectorTest, ProxyConfigDefaultValues) {
	proxy_connection_config config;

	EXPECT_EQ(config.server_host, "localhost");
	EXPECT_EQ(config.server_port, 9432);
	EXPECT_TRUE(config.auth_token.empty());
	EXPECT_EQ(config.connection_timeout, std::chrono::milliseconds{5000});
	EXPECT_EQ(config.query_timeout, std::chrono::milliseconds{30000});
	EXPECT_EQ(config.retry_count, 3);
	EXPECT_EQ(config.retry_delay, std::chrono::milliseconds{1000});
	EXPECT_TRUE(config.use_tls);
}

TEST_F(ProxyConnectorTest, ProxyConfigValidation) {
	// Valid config
	auto valid_config = create_valid_config();
	EXPECT_TRUE(valid_config.is_valid());

	// Invalid: empty host
	proxy_connection_config invalid_host;
	invalid_host.server_host = "";
	EXPECT_FALSE(invalid_host.is_valid());

	// Invalid: zero port
	proxy_connection_config invalid_port;
	invalid_port.server_port = 0;
	EXPECT_FALSE(invalid_port.is_valid());

	// Invalid: zero timeout
	proxy_connection_config invalid_timeout;
	invalid_timeout.connection_timeout = std::chrono::milliseconds{0};
	EXPECT_FALSE(invalid_timeout.is_valid());
}

// proxy_connector tests
TEST_F(ProxyConnectorTest, ProxyConnectorConstruction) {
	auto config = create_valid_config();
	auto connector = std::make_unique<proxy_connector>(
		database_types::postgres, config);

	EXPECT_NE(connector, nullptr);
	EXPECT_EQ(connector->database_type(), database_types::postgres);
	EXPECT_EQ(connector->state(), proxy_state::disconnected);
	EXPECT_FALSE(connector->is_connected());
}

TEST_F(ProxyConnectorTest, ProxyConnectorStateToString) {
	EXPECT_STREQ(to_string(proxy_state::disconnected), "disconnected");
	EXPECT_STREQ(to_string(proxy_state::connecting), "connecting");
	EXPECT_STREQ(to_string(proxy_state::connected), "connected");
	EXPECT_STREQ(to_string(proxy_state::error), "error");
}

TEST_F(ProxyConnectorTest, ProxyConnectorConnect) {
	auto config = create_valid_config();
	auto connector = std::make_unique<proxy_connector>(
		database_types::postgres, config);

	// Connection should fail (stub implementation, server doesn't exist)
	bool connected = connector->connect("");
	EXPECT_FALSE(connected);
	EXPECT_FALSE(connector->is_connected());

	// Should have error message
	std::string error = connector->last_error();
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(error.find("not available") != std::string::npos ||
				error.find("stub") != std::string::npos);
}

TEST_F(ProxyConnectorTest, ProxyConnectorMoveSemantics) {
	auto config = create_valid_config();
	auto connector1 = std::make_unique<proxy_connector>(
		database_types::mysql, config);

	EXPECT_EQ(connector1->database_type(), database_types::mysql);

	// Move constructor
	proxy_connector connector2 = std::move(*connector1);
	EXPECT_EQ(connector2.database_type(), database_types::mysql);
	EXPECT_EQ(connector2.state(), proxy_state::disconnected);
}

// database_manager proxy mode tests
TEST_F(ProxyConnectorTest, DatabaseManagerDefaultMode) {
	// Default should be direct mode
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::direct);
}

TEST_F(ProxyConnectorTest, DatabaseManagerSetModeProxy) {
	auto config = create_valid_config();

	// Set proxy mode
	bool result = db_mgr_->set_mode_proxy(database_types::postgres, config);
	EXPECT_TRUE(result);
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::proxy);
	EXPECT_EQ(db_mgr_->database_type(), database_types::postgres);
}

TEST_F(ProxyConnectorTest, DatabaseManagerSetModeProxyInvalidConfig) {
	proxy_connection_config invalid_config;
	invalid_config.server_host = "";  // Invalid

	// Should fail with invalid config
	bool result = db_mgr_->set_mode_proxy(database_types::postgres, invalid_config);
	EXPECT_FALSE(result);
	// Should remain in direct mode
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::direct);
}

TEST_F(ProxyConnectorTest, DatabaseManagerSetModeDirectAfterProxy) {
	auto config = create_valid_config();

	// Set proxy mode
	EXPECT_TRUE(db_mgr_->set_mode_proxy(database_types::postgres, config));
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::proxy);

	// Switch back to direct mode
	EXPECT_TRUE(db_mgr_->set_mode(database_types::mysql));
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::direct);
}

TEST_F(ProxyConnectorTest, DatabaseManagerProxyModeConnect) {
	auto config = create_valid_config();

	// Set proxy mode
	EXPECT_TRUE(db_mgr_->set_mode_proxy(database_types::postgres, config));

	// Connect should fail (stub, no server)
	bool connected = db_mgr_->connect("");
	EXPECT_FALSE(connected);
}

// proxy_server_info tests
TEST_F(ProxyConnectorTest, ProxyServerInfoDefaultValues) {
	proxy_server_info info;

	EXPECT_TRUE(info.version.empty());
	EXPECT_TRUE(info.server_id.empty());
	EXPECT_TRUE(info.supported_databases.empty());
	EXPECT_EQ(info.max_connections, 0);
	EXPECT_FALSE(info.tls_enabled);
}

TEST_F(ProxyConnectorTest, ProxyConnectorNoServerInfo) {
	auto config = create_valid_config();
	auto connector = std::make_unique<proxy_connector>(
		database_types::postgres, config);

	// No server info when not connected
	auto server_info = connector->server_info();
	EXPECT_FALSE(server_info.has_value());
}

// Thread safety tests
TEST_F(ProxyConnectorTest, ProxyConfigAccessThreadSafe) {
	auto config = create_valid_config();
	auto connector = std::make_unique<proxy_connector>(
		database_types::postgres, config);

	// Access config from multiple threads
	std::vector<std::thread> threads;
	for (int i = 0; i < 10; ++i) {
		threads.emplace_back([&connector]() {
			const auto& cfg = connector->config();
			EXPECT_EQ(cfg.server_port, 9432);
		});
	}

	for (auto& t : threads) {
		t.join();
	}
}

// Backward compatibility tests
TEST_F(ProxyConnectorTest, ExistingAPIBackwardCompatibility) {
	// Verify existing API still works

	// set_mode should work as before
	EXPECT_TRUE(db_mgr_->set_mode(database_types::postgres));
	EXPECT_EQ(db_mgr_->database_type(), database_types::postgres);
	EXPECT_EQ(db_mgr_->current_connection_mode(), connection_mode::direct);

	// All other methods should work
	EXPECT_NO_THROW(db_mgr_->create_query("SELECT 1"));
	EXPECT_NO_THROW(db_mgr_->select_query("SELECT 1"));
	EXPECT_NO_THROW(db_mgr_->disconnect());
}
