// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "database_backend.h"

#include <sstream>
#include <algorithm>

namespace database
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
} // namespace database
