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

#include "mysql_manager.h"

#ifdef USE_MYSQL
#include <mysql.h>
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>

// Logging helper macros - using callback if set, otherwise std::cerr/cout
// This design allows integration with logger_adapter from integrated_database
// module via runtime callback injection, avoiding circular dependencies.
// Note: These macros must be used within mysql_manager member functions
// where logger_callback_ is accessible.
#define MYSQL_LOG_ERROR(context, message)                                      \
  do {                                                                         \
    if (logger_callback_) {                                                    \
      logger_callback_(mysql_log_level::error, context, message);              \
    } else {                                                                   \
      std::cerr << "[MySQL:" << context << "] Error: " << message              \
                << std::endl;                                                  \
    }                                                                          \
  } while (0)

#define MYSQL_LOG_WARNING(message)                                             \
  do {                                                                         \
    if (logger_callback_) {                                                    \
      logger_callback_(mysql_log_level::warning, "warning", message);          \
    } else {                                                                   \
      std::cerr << "[MySQL] Warning: " << message << std::endl;                \
    }                                                                          \
  } while (0)

#define MYSQL_LOG_INFO(message)                                                \
  do {                                                                         \
    if (logger_callback_) {                                                    \
      logger_callback_(mysql_log_level::info, "info", message);                \
    } else {                                                                   \
      std::cout << "[MySQL] Info: " << message << std::endl;                   \
    }                                                                          \
  } while (0)

namespace database {
mysql_manager::mysql_manager(void) : connection_(nullptr) {}

mysql_manager::~mysql_manager(void) { disconnect(); }

database_types mysql_manager::database_type(void) {
  return database_types::mysql;
}

void mysql_manager::set_logger(mysql_logger_callback callback) {
  logger_callback_ = std::move(callback);
}

bool mysql_manager::connect(const std::string &connect_string) {
#ifdef USE_MYSQL
  try {
    // Parse connection string
    std::string host, database, user, password;
    unsigned int port;
    if (!parse_connection_string(connect_string, host, port, database, user,
                                 password)) {
      MYSQL_LOG_ERROR("connect", "Connection string parsing failed");
      return false;
    }

    // Initialize MySQL connection
    MYSQL *mysql = mysql_init(nullptr);
    if (!mysql) {
      MYSQL_LOG_ERROR("connect", "MySQL library initialization failed");
      return false;
    }

    // Attempt to connect
    connection_ =
        mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0);

    if (!connection_) {
      MYSQL_LOG_ERROR("connect",
                      std::string("Connection failed: ") + mysql_error(mysql));
      mysql_close(mysql);
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    MYSQL_LOG_ERROR("connect", std::string("Connection error: ") + e.what());
  }
#else
  MYSQL_LOG_WARNING("MySQL support not compiled. Connection: " +
                    connect_string.substr(0, 20) + "...");
#endif
  return false;
}

bool mysql_manager::create_query(const std::string &query_string) {
#ifdef USE_MYSQL
  if (!connection_)
    return false;
  try {
    MYSQL *mysql = static_cast<MYSQL *>(connection_);
    int result = mysql_query(mysql, query_string.c_str());
    if (result != 0) {
      MYSQL_LOG_ERROR("create_query", std::string("Query execution error: ") +
                                          mysql_error(mysql));
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    MYSQL_LOG_ERROR("create_query",
                    std::string("Query execution error: ") + e.what());
  }
#else
  MYSQL_LOG_WARNING("MySQL support not compiled. Query: " +
                    query_string.substr(0, 20) + "...");
#endif
  return false;
}

unsigned int mysql_manager::insert_query(const std::string &query_string) {
  return execute_modification_query(query_string);
}

unsigned int mysql_manager::update_query(const std::string &query_string) {
  return execute_modification_query(query_string);
}

unsigned int mysql_manager::delete_query(const std::string &query_string) {
  return execute_modification_query(query_string);
}

database_result mysql_manager::select_query(const std::string &query_string) {
  database_result result;
#ifdef USE_MYSQL
  if (!connection_)
    return result;
  try {
    MYSQL *mysql = static_cast<MYSQL *>(connection_);

    // Execute query
    if (mysql_query(mysql, query_string.c_str()) != 0) {
      MYSQL_LOG_ERROR("select_query",
                      std::string("Query failed: ") + mysql_error(mysql));
      return result;
    }

    // Get result set
    MYSQL_RES *res = mysql_store_result(mysql);
    if (!res) {
      if (mysql_field_count(mysql) == 0) {
        // Query was not a SELECT
        return result;
      } else {
        MYSQL_LOG_ERROR("select_query",
                        std::string("Result retrieval failed: ") +
                            mysql_error(mysql));
        return result;
      }
    }

    // Get field information
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    unsigned int num_fields = mysql_num_fields(res);

    // Process rows
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
      database_row db_row;
      unsigned long *lengths = mysql_fetch_lengths(res);

      for (unsigned int i = 0; i < num_fields; i++) {
        std::string field_name = fields[i].name;

        if (row[i] == nullptr) {
          db_row[field_name] = nullptr;
        } else {
          // Convert MySQL types to database_value
          switch (fields[i].type) {
          case MYSQL_TYPE_TINY:
          case MYSQL_TYPE_SHORT:
          case MYSQL_TYPE_LONG:
          case MYSQL_TYPE_LONGLONG:
          case MYSQL_TYPE_INT24:
            db_row[field_name] = static_cast<int64_t>(std::stoll(row[i]));
            break;
          case MYSQL_TYPE_DECIMAL:
          case MYSQL_TYPE_NEWDECIMAL:
          case MYSQL_TYPE_FLOAT:
          case MYSQL_TYPE_DOUBLE:
            db_row[field_name] = std::stod(row[i]);
            break;
          case MYSQL_TYPE_BIT:
            db_row[field_name] = (row[i][0] != '0');
            break;
          default:
            db_row[field_name] = std::string(row[i], lengths[i]);
            break;
          }
        }
      }
      result.push_back(std::move(db_row));
    }

    mysql_free_result(res);
  } catch (const std::exception &e) {
    MYSQL_LOG_ERROR("select_query", std::string("Query error: ") + e.what());
  }
#else
  MYSQL_LOG_WARNING("MySQL support not compiled. Query: " +
                    query_string.substr(0, 20) + "...");
  // Mock data for testing
  if (query_string.find("SELECT") != std::string::npos) {
    database_row mock_row;
    mock_row["id"] = int64_t(1);
    mock_row["name"] = std::string("mysql_mock_data");
    result.push_back(mock_row);
  }
#endif
  return result;
}

bool mysql_manager::execute_query(const std::string &query_string) {
#ifdef USE_MYSQL
  if (!connection_) {
    MYSQL_LOG_ERROR("execute_query", "No active MySQL connection");
    return false;
  }

  if (mysql_query(static_cast<MYSQL *>(connection_), query_string.c_str()) !=
      0) {
    MYSQL_LOG_ERROR("execute_query",
                    std::string("Execute error: ") +
                        mysql_error(static_cast<MYSQL *>(connection_)));
    return false;
  }

  return true;
#else
  // Mock execution
  MYSQL_LOG_INFO("MySQL support not compiled. Mock execute: " + query_string);
  return true;
#endif
}

bool mysql_manager::disconnect(void) {
#ifdef USE_MYSQL
  if (connection_) {
    MYSQL *mysql = static_cast<MYSQL *>(connection_);
    mysql_close(mysql);
    connection_ = nullptr;
    return true;
  }
#endif
  return false;
}

void *mysql_manager::query_result(const std::string &query_string) {
#ifdef USE_MYSQL
  if (!connection_)
    return nullptr;
  try {
    MYSQL *mysql = static_cast<MYSQL *>(connection_);
    if (mysql_query(mysql, query_string.c_str()) != 0) {
      return nullptr;
    }
    return mysql_store_result(mysql);
  } catch (const std::exception &e) {
    MYSQL_LOG_ERROR("query_result",
                    std::string("Query result error: ") + e.what());
  }
#endif
  return nullptr;
}

unsigned int
mysql_manager::execute_modification_query(const std::string &query_string) {
#ifdef USE_MYSQL
  if (!connection_)
    return 0;
  try {
    MYSQL *mysql = static_cast<MYSQL *>(connection_);
    if (mysql_query(mysql, query_string.c_str()) != 0) {
      MYSQL_LOG_ERROR("execute_modification_query",
                      std::string("Modification query failed: ") +
                          mysql_error(mysql));
      return 0;
    }
    return static_cast<unsigned int>(mysql_affected_rows(mysql));
  } catch (const std::exception &e) {
    MYSQL_LOG_ERROR("execute_modification_query",
                    std::string("Modification query error: ") + e.what());
  }
#else
  MYSQL_LOG_WARNING("MySQL support not compiled. Modification query: " +
                    query_string.substr(0, 20) + "...");
#endif
  return 0;
}

bool mysql_manager::parse_connection_string(
    const std::string &connect_string, std::string &host, unsigned int &port,
    std::string &database, std::string &user, std::string &password) {
  // Default values
  host = "localhost";
  port = 3306;
  database = "";
  user = "";
  password = "";

  // Parse connection string format:
  // "host=value;port=value;database=value;user=value;password=value"
  std::istringstream ss(connect_string);
  std::string pair;

  while (std::getline(ss, pair, ';')) {
    size_t eq_pos = pair.find('=');
    if (eq_pos == std::string::npos)
      continue;

    std::string key = pair.substr(0, eq_pos);
    std::string value = pair.substr(eq_pos + 1);

    if (key == "host") {
      host = value;
    } else if (key == "port") {
      port = static_cast<unsigned int>(std::stoul(value));
    } else if (key == "database") {
      database = value;
    } else if (key == "user") {
      user = value;
    } else if (key == "password") {
      password = value;
    }
  }

  // Validate required fields
  return !database.empty() && !user.empty();
}
} // namespace database