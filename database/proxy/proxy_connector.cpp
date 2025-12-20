/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "proxy_connector.h"

#include <sstream>

namespace database
{
namespace proxy
{

proxy_connector::proxy_connector(database_types db_type,
								 const proxy_connection_config& config)
	: db_type_(db_type)
	, config_(config)
	, state_(proxy_state::disconnected)
	, last_error_()
	, server_info_(std::nullopt)
{
}

proxy_connector::~proxy_connector()
{
	if (is_connected()) {
		disconnect();
	}
}

proxy_connector::proxy_connector(proxy_connector&& other) noexcept
	: db_type_(other.db_type_)
	, config_(std::move(other.config_))
	, state_(other.state_.load())
	, last_error_(std::move(other.last_error_))
	, server_info_(std::move(other.server_info_))
{
	other.state_ = proxy_state::disconnected;
}

proxy_connector& proxy_connector::operator=(proxy_connector&& other) noexcept
{
	if (this != &other) {
		if (is_connected()) {
			disconnect();
		}

		std::lock_guard<std::mutex> lock(mutex_);
		db_type_ = other.db_type_;
		config_ = std::move(other.config_);
		state_ = other.state_.load();
		last_error_ = std::move(other.last_error_);
		server_info_ = std::move(other.server_info_);

		other.state_ = proxy_state::disconnected;
	}
	return *this;
}

database_types proxy_connector::database_type()
{
	return db_type_;
}

bool proxy_connector::connect(const std::string& /*connect_string*/)
{
	// In proxy mode, the connect_string is ignored.
	// Connection parameters are taken from proxy_connection_config.

	if (!config_.is_valid()) {
		set_error("Invalid proxy configuration");
		return false;
	}

	return try_connect();
}

bool proxy_connector::try_connect()
{
	state_ = proxy_state::connecting;

	// TODO: Phase 1-3 - Implement actual network connection to database_server
	// This is a stub implementation for Phase 4.1
	//
	// When database_server is available, this will:
	// 1. Establish TCP/QUIC connection to config_.server_host:config_.server_port
	// 2. Perform TLS handshake if config_.use_tls is true
	// 3. Authenticate using config_.auth_token
	// 4. Receive server_info_ from handshake response

	// For now, simulate connection failure since server doesn't exist
	std::ostringstream oss;
	oss << "database_server not available at "
		<< config_.server_host << ":" << config_.server_port
		<< " (stub implementation - server not yet implemented)";

	set_error(oss.str());
	state_ = proxy_state::error;

	return false;
}

bool proxy_connector::create_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return false;
	}

	// TODO: Phase 1-3 - Send prepared statement request to server
	auto result = send_query("PREPARE", query_string);
	return !result.empty();
}

unsigned int proxy_connector::insert_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return 0;
	}

	// TODO: Phase 1-3 - Send INSERT query to server and parse affected rows
	send_query("INSERT", query_string);
	return 0;
}

unsigned int proxy_connector::update_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return 0;
	}

	// TODO: Phase 1-3 - Send UPDATE query to server and parse affected rows
	send_query("UPDATE", query_string);
	return 0;
}

unsigned int proxy_connector::delete_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return 0;
	}

	// TODO: Phase 1-3 - Send DELETE query to server and parse affected rows
	send_query("DELETE", query_string);
	return 0;
}

database_result proxy_connector::select_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return {};
	}

	// TODO: Phase 1-3 - Send SELECT query to server and deserialize results
	return send_query("SELECT", query_string);
}

bool proxy_connector::execute_query(const std::string& query_string)
{
	if (!is_connected()) {
		set_error("Not connected to database_server");
		return false;
	}

	// TODO: Phase 1-3 - Send general query to server
	auto result = send_query("EXECUTE", query_string);
	return true; // Stub always returns true if connected
}

bool proxy_connector::disconnect()
{
	if (!is_connected()) {
		return true; // Already disconnected
	}

	std::lock_guard<std::mutex> lock(mutex_);

	// TODO: Phase 1-3 - Send disconnect message to server and close socket

	state_ = proxy_state::disconnected;
	server_info_ = std::nullopt;
	last_error_.clear();

	return true;
}

proxy_state proxy_connector::state() const noexcept
{
	return state_.load();
}

bool proxy_connector::is_connected() const noexcept
{
	return state_.load() == proxy_state::connected;
}

std::optional<proxy_server_info> proxy_connector::server_info() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return server_info_;
}

std::string proxy_connector::last_error() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return last_error_;
}

const proxy_connection_config& proxy_connector::config() const noexcept
{
	return config_;
}

database_result proxy_connector::send_query(const std::string& /*query_type*/,
											const std::string& /*query_string*/)
{
	// TODO: Phase 1-3 - Implement actual query sending
	//
	// Protocol outline:
	// 1. Serialize request: { type: query_type, db_type: db_type_, query: query_string }
	// 2. Send to server via established connection
	// 3. Wait for response with timeout (config_.query_timeout)
	// 4. Deserialize response to database_result
	// 5. Handle errors and retries

	// Stub implementation returns empty result
	return {};
}

void proxy_connector::set_error(const std::string& message)
{
	std::lock_guard<std::mutex> lock(mutex_);
	last_error_ = message;
}

} // namespace proxy
} // namespace database
