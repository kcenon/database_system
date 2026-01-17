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

/**
 * @file test_connection_string_builder.cpp
 * @brief Unit tests for connection_string_builder
 */

#include "integrated/connection_string_builder.h"

#include <iostream>
#include <string>

using namespace database::integrated;

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

// Test helpers
#define TEST_START(name) \
    std::cout << "\n[TEST] " << name << "...\n"

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << "  FAILED: " << message << "\n"; \
            std::cout << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition, message) \
    ASSERT_TRUE(!(condition), message)

#define ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            std::cout << "  FAILED: " << message << "\n"; \
            std::cout << "     Expected: \"" << (expected) << "\"\n"; \
            std::cout << "     Actual:   \"" << (actual) << "\"\n"; \
            std::cout << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_END() \
    do { \
        std::cout << "  PASSED\n"; \
        tests_passed++; \
        return true; \
    } while(0)

//==============================================================================
// PostgreSQL Tests
//==============================================================================

bool test_postgres_basic() {
    TEST_START("PostgreSQL - Basic Connection String");

    auto result = connection_string_builder()
        .host("localhost")
        .port(5432)
        .database("mydb")
        .user("admin")
        .password("secret")
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");

    auto conn_str = result.value();
    ASSERT_TRUE(conn_str.find("host=localhost") != std::string::npos, "Should contain host");
    ASSERT_TRUE(conn_str.find("port=5432") != std::string::npos, "Should contain port");
    ASSERT_TRUE(conn_str.find("dbname=mydb") != std::string::npos, "Should contain dbname");
    ASSERT_TRUE(conn_str.find("user=admin") != std::string::npos, "Should contain user");
    ASSERT_TRUE(conn_str.find("password=secret") != std::string::npos, "Should contain password");

    TEST_END();
}

bool test_postgres_ssl() {
    TEST_START("PostgreSQL - SSL Mode");

    auto result = connection_string_builder()
        .host("localhost")
        .database("mydb")
        .ssl_mode(ssl_mode::require)
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("sslmode=require") != std::string::npos, "Should contain sslmode");

    TEST_END();
}

bool test_postgres_verify_ca() {
    TEST_START("PostgreSQL - SSL Verify CA");

    auto result = connection_string_builder()
        .host("localhost")
        .ssl_mode(ssl_mode::verify_ca)
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("sslmode=verify-ca") != std::string::npos, "Should contain verify-ca");

    TEST_END();
}

bool test_postgres_timeout() {
    TEST_START("PostgreSQL - Connection Timeout");

    auto result = connection_string_builder()
        .host("localhost")
        .connect_timeout(30)
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("connect_timeout=30") != std::string::npos, "Should contain timeout");

    TEST_END();
}

bool test_postgres_app_name() {
    TEST_START("PostgreSQL - Application Name");

    auto result = connection_string_builder()
        .host("localhost")
        .application_name("my_app")
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("application_name=my_app") != std::string::npos, "Should contain app name");

    TEST_END();
}

bool test_postgres_custom_option() {
    TEST_START("PostgreSQL - Custom Option");

    auto result = connection_string_builder()
        .host("localhost")
        .option("client_encoding", "UTF8")
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("client_encoding=UTF8") != std::string::npos, "Should contain custom option");

    TEST_END();
}

bool test_postgres_empty() {
    TEST_START("PostgreSQL - Empty Builder");

    auto result = connection_string_builder()
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Build should succeed even with empty builder");
    ASSERT_EQ("", result.value(), "Empty builder should produce empty string");

    TEST_END();
}

//==============================================================================
// MySQL Tests
//==============================================================================

bool test_mysql_basic() {
    TEST_START("MySQL - Basic Connection String");

    auto result = connection_string_builder()
        .host("localhost")
        .port(3306)
        .database("mydb")
        .user("admin")
        .password("secret")
        .build(backend_type::mysql);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");

    auto conn_str = result.value();
    ASSERT_TRUE(conn_str.find("host=localhost") != std::string::npos, "Should contain host");
    ASSERT_TRUE(conn_str.find("port=3306") != std::string::npos, "Should contain port");
    ASSERT_TRUE(conn_str.find("database=mydb") != std::string::npos, "Should contain database");
    ASSERT_TRUE(conn_str.find("user=admin") != std::string::npos, "Should contain user");
    ASSERT_TRUE(conn_str.find("password=secret") != std::string::npos, "Should contain password");
    ASSERT_TRUE(conn_str.find(";") != std::string::npos, "MySQL should use semicolon separator");

    TEST_END();
}

bool test_mysql_ssl() {
    TEST_START("MySQL - SSL Mode");

    auto result = connection_string_builder()
        .host("localhost")
        .ssl_mode(ssl_mode::require)
        .build(backend_type::mysql);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("sslmode=REQUIRED") != std::string::npos, "Should contain SSL mode");

    TEST_END();
}

//==============================================================================
// SQLite Tests
//==============================================================================

bool test_sqlite_file() {
    TEST_START("SQLite - File Database");

    auto result = connection_string_builder()
        .database("mydb.db")
        .build(backend_type::sqlite);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("mydb.db", result.value(), "Should return file path");

    TEST_END();
}

bool test_sqlite_memory() {
    TEST_START("SQLite - In-Memory Database");

    auto result = connection_string_builder()
        .in_memory()
        .build(backend_type::sqlite);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ(":memory:", result.value(), "Should return :memory:");

    TEST_END();
}

bool test_sqlite_no_database() {
    TEST_START("SQLite - No Database Set");

    auto result = connection_string_builder()
        .build(backend_type::sqlite);

    ASSERT_TRUE(result.is_err(), "Build should fail without database");

    TEST_END();
}

bool test_sqlite_memory_overrides_file() {
    TEST_START("SQLite - In-Memory Overrides File");

    auto result = connection_string_builder()
        .database("mydb.db")
        .in_memory()
        .build(backend_type::sqlite);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ(":memory:", result.value(), "In-memory should override file");

    TEST_END();
}

//==============================================================================
// MongoDB Tests
//==============================================================================

bool test_mongodb_basic() {
    TEST_START("MongoDB - Basic Connection String");

    auto result = connection_string_builder()
        .host("localhost")
        .port(27017)
        .database("mydb")
        .build(backend_type::mongodb);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("mongodb://localhost:27017/mydb", result.value(), "Should return MongoDB URI");

    TEST_END();
}

bool test_mongodb_with_auth() {
    TEST_START("MongoDB - With Authentication");

    auto result = connection_string_builder()
        .host("localhost")
        .user("admin")
        .password("secret")
        .database("mydb")
        .build(backend_type::mongodb);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("mongodb://admin:secret@localhost/mydb", result.value(), "Should include credentials");

    TEST_END();
}

bool test_mongodb_default_host() {
    TEST_START("MongoDB - Default Host");

    auto result = connection_string_builder()
        .database("mydb")
        .build(backend_type::mongodb);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("localhost") != std::string::npos, "Should default to localhost");

    TEST_END();
}

bool test_mongodb_ssl() {
    TEST_START("MongoDB - SSL Option");

    auto result = connection_string_builder()
        .host("localhost")
        .ssl_mode(ssl_mode::require)
        .build(backend_type::mongodb);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_TRUE(result.value().find("ssl=true") != std::string::npos, "Should include SSL option");

    TEST_END();
}

//==============================================================================
// Redis Tests
//==============================================================================

bool test_redis_basic() {
    TEST_START("Redis - Basic Connection String");

    auto result = connection_string_builder()
        .host("localhost")
        .port(6379)
        .build(backend_type::redis);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("redis://localhost:6379", result.value(), "Should return Redis URI");

    TEST_END();
}

bool test_redis_with_password() {
    TEST_START("Redis - With Password Only");

    auto result = connection_string_builder()
        .host("localhost")
        .password("secret")
        .build(backend_type::redis);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("redis://:secret@localhost", result.value(), "Should include password");

    TEST_END();
}

bool test_redis_with_database() {
    TEST_START("Redis - With Database Number");

    auto result = connection_string_builder()
        .host("localhost")
        .database("0")
        .build(backend_type::redis);

    ASSERT_TRUE(result.is_ok(), "Build should succeed");
    ASSERT_EQ("redis://localhost/0", result.value(), "Should include database number");

    TEST_END();
}

//==============================================================================
// Builder Behavior Tests
//==============================================================================

bool test_builder_reset() {
    TEST_START("Builder - Reset");

    connection_string_builder builder;
    builder.host("localhost")
           .port(5432)
           .database("mydb");

    auto result1 = builder.build(backend_type::postgres);
    ASSERT_TRUE(result1.is_ok(), "First build should succeed");
    ASSERT_TRUE(result1.value().find("localhost") != std::string::npos, "Should contain host");

    builder.reset();

    auto result2 = builder.build(backend_type::postgres);
    ASSERT_TRUE(result2.is_ok(), "Build after reset should succeed");
    ASSERT_EQ("", result2.value(), "After reset should be empty");

    TEST_END();
}

bool test_builder_chaining() {
    TEST_START("Builder - Method Chaining");

    auto result = connection_string_builder()
        .host("db.example.com")
        .port(5432)
        .database("production")
        .user("app_user")
        .password("app_secret")
        .ssl_mode(ssl_mode::verify_full)
        .connect_timeout(10)
        .application_name("my_service")
        .option("target_session_attrs", "read-write")
        .build(backend_type::postgres);

    ASSERT_TRUE(result.is_ok(), "Chained build should succeed");

    auto conn_str = result.value();
    ASSERT_TRUE(conn_str.find("host=db.example.com") != std::string::npos, "Should have host");
    ASSERT_TRUE(conn_str.find("sslmode=verify-full") != std::string::npos, "Should have SSL mode");
    ASSERT_TRUE(conn_str.find("target_session_attrs=read-write") != std::string::npos, "Should have custom option");

    TEST_END();
}

bool test_builder_reuse() {
    TEST_START("Builder - Reuse for Multiple Connections");

    connection_string_builder builder;
    builder.host("localhost")
           .user("admin")
           .password("secret");

    // Build for PostgreSQL
    builder.database("pg_db").port(5432);
    auto pg_result = builder.build(backend_type::postgres);
    ASSERT_TRUE(pg_result.is_ok(), "PostgreSQL build should succeed");

    // Reset and build for MySQL
    builder.reset()
           .host("localhost")
           .user("admin")
           .password("secret")
           .database("mysql_db")
           .port(3306);
    auto mysql_result = builder.build(backend_type::mysql);
    ASSERT_TRUE(mysql_result.is_ok(), "MySQL build should succeed");

    TEST_END();
}

bool test_builder_copy() {
    TEST_START("Builder - Copy Semantics");

    connection_string_builder builder1;
    builder1.host("localhost").port(5432);

    connection_string_builder builder2 = builder1;
    builder2.database("mydb");

    auto result1 = builder1.build(backend_type::postgres);
    auto result2 = builder2.build(backend_type::postgres);

    ASSERT_TRUE(result1.is_ok(), "Original build should succeed");
    ASSERT_TRUE(result2.is_ok(), "Copy build should succeed");

    // Original should not have database
    ASSERT_TRUE(result1.value().find("dbname") == std::string::npos, "Original should not have database");
    // Copy should have database
    ASSERT_TRUE(result2.value().find("dbname=mydb") != std::string::npos, "Copy should have database");

    TEST_END();
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Connection String Builder Tests\n";
    std::cout << "========================================\n";

    // PostgreSQL tests
    test_postgres_basic();
    test_postgres_ssl();
    test_postgres_verify_ca();
    test_postgres_timeout();
    test_postgres_app_name();
    test_postgres_custom_option();
    test_postgres_empty();

    // MySQL tests
    test_mysql_basic();
    test_mysql_ssl();

    // SQLite tests
    test_sqlite_file();
    test_sqlite_memory();
    test_sqlite_no_database();
    test_sqlite_memory_overrides_file();

    // MongoDB tests
    test_mongodb_basic();
    test_mongodb_with_auth();
    test_mongodb_default_host();
    test_mongodb_ssl();

    // Redis tests
    test_redis_basic();
    test_redis_with_password();
    test_redis_with_database();

    // Builder behavior tests
    test_builder_reset();
    test_builder_chaining();
    test_builder_reuse();
    test_builder_copy();

    // Print summary
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";

    if (tests_failed == 0) {
        std::cout << "\nAll tests passed!\n\n";
        return 0;
    } else {
        std::cout << "\nSome tests failed!\n\n";
        return 1;
    }
}
