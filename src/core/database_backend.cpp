// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/core/database_backend.h>

#include <sstream>
#include <algorithm>

namespace kcenon::database
{
namespace core
{

connection_config connection_config::from_string(const std::string& connect_string)
{
	connection_config config;

	// Parse connection string in key=value format
	// Example: "host=localhost port=5432 dbname=testdb user=postgres password=secret"

	std::istringstream stream(connect_string);
	std::string token;

	while (stream >> token)
	{
		// Find '=' separator
		size_t pos = token.find('=');
		if (pos == std::string::npos)
		{
			continue; // Skip malformed tokens
		}

		std::string key = token.substr(0, pos);
		std::string value = token.substr(pos + 1);

		// Remove quotes if present
		if (!value.empty() && value.front() == '\'' && value.back() == '\'')
		{
			value = value.substr(1, value.length() - 2);
		}

		// Map common keys to config fields
		if (key == "host" || key == "hostname")
		{
			config.host = value;
		}
		else if (key == "port")
		{
			try
			{
				config.port = static_cast<uint16_t>(std::stoi(value));
			}
			catch (...)
			{
				// Invalid port, use default
			}
		}
		else if (key == "database" || key == "dbname" || key == "db")
		{
			config.database = value;
		}
		else if (key == "user" || key == "username")
		{
			config.username = value;
		}
		else if (key == "password" || key == "pass" || key == "pwd")
		{
			config.password = value;
		}
		else
		{
			// Store other options
			config.options[key] = value;
		}
	}

	return config;
}

} // namespace core
} // namespace kcenon::database
