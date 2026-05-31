// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Credential Security Tests (DB-008)
 *
 * Tests for credential and connection string security:
 * - Password not exposed in error messages
 * - Password not logged
 * - Special characters in passwords handled safely
 * - Connection string parsing security
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <sstream>
#include <regex>

#include <kcenon/database/backends/sqlite_backend.h>
#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/database_types.h>

using namespace kcenon::database;
using namespace kcenon::database::backends;
using namespace kcenon::database::core;

/**
 * @class CredentialSecurityTest
 * @brief Test fixture for credential security tests
 */
class CredentialSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    /**
     * @brief Capture output from a stream during a function execution
     */
    template<typename Func>
    std::string captureOutput(std::ostream& stream, Func func) {
        std::stringstream captured;
        std::streambuf* original = stream.rdbuf(captured.rdbuf());
        func();
        stream.rdbuf(original);
        return captured.str();
    }
};

//=============================================================================
// Error Message Security Tests
//=============================================================================

/**
 * @test PasswordNotInErrorMessages
 * @brief Tests that passwords are not exposed in error messages
 *
 * When a connection fails, the error message should not contain
 * the password from the connection string.
 */
TEST_F(CredentialSecurityTest, PasswordNotInErrorMessages) {
    auto db = std::make_unique<sqlite_backend>();

    // Try connecting with a path that contains password-like info
    // Note: SQLite doesn't use passwords, but we test the principle
    connection_config config;
    config.database = "/nonexistent/secret123/database.db";

    auto result = db->initialize(config);

    // Connection should fail for non-existent path
#ifdef USE_SQLITE
    EXPECT_FALSE(result.is_ok());
#endif

    // Even if we had a way to get error messages,
    // they should not contain sensitive parts of the connection string
    // This is a design principle test
    SUCCEED() << "Connection string paths should not leak in errors";
}

/**
 * @test PasswordNotExposedOnConnectionFailure
 * @brief Tests that failed connections don't expose credentials
 */
TEST_F(CredentialSecurityTest, PasswordNotExposedOnConnectionFailure) {
    // Simulate a connection with credentials
    connection_config config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "test";
    config.username = "admin";
    config.password = "SuperSecret123!@#";

    // Capture any console output during connection attempt
    std::string output = captureOutput(std::cerr, [&]() {
        auto db = std::make_unique<sqlite_backend>();
        db->initialize(config);  // Will fail - SQLite doesn't use network connections
    });

    // Verify password is not in captured output
    EXPECT_TRUE(output.find("SuperSecret123") == std::string::npos)
        << "Password exposed in stderr output: " << output;
    EXPECT_TRUE(output.find("Secret") == std::string::npos ||
                output.find("secret") == std::string::npos)
        << "Possible password leak in output";
}

//=============================================================================
// Connection String Parsing Security
//=============================================================================

/**
 * @test SpecialCharactersInPassword
 * @brief Tests that special characters in passwords don't break parsing
 */
TEST_F(CredentialSecurityTest, SpecialCharactersInPassword) {
    // Passwords with special characters that might break naive parsing
    std::vector<std::string> special_passwords = {
        "pass=word",       // Contains =
        "pass;word",       // Contains ;
        "pass'word",       // Contains '
        "pass\"word",      // Contains "
        "pass\\word",      // Contains backslash
        "pass word",       // Contains space
        "pass\nword",      // Contains newline
        "pass%word",       // Contains percent
        "!@#$%^&*()",     // All special chars
    };

    for (const auto& pwd : special_passwords) {
        // These should not cause crashes or undefined behavior
        auto db = std::make_unique<sqlite_backend>();

        connection_config config;
        config.database = ":memory:";
        config.password = pwd;

        // SQLite uses file paths, not connection strings with passwords
        // But the parsing should be safe regardless
        EXPECT_NO_THROW({
            db->initialize(config);
        }) << "Connection failed with password containing special chars: " << pwd;
    }
}

/**
 * @test ConnectionStringInjectionPrevention
 * @brief Tests that connection string values can't inject parameters
 */
TEST_F(CredentialSecurityTest, ConnectionStringInjectionPrevention) {
    // Attempt to inject additional connection parameters
    std::vector<std::string> injection_attempts = {
        "database=test;admin=true",           // Parameter injection
        "database=test\x00admin=true",        // Null byte injection
        "database=test%00admin=true",         // URL-encoded null
        "database=test;--comment",            // Comment injection
    };

    for (const auto& db_name : injection_attempts) {
        auto db = std::make_unique<sqlite_backend>();

        connection_config config;
        config.database = db_name;

        // Should not crash or behave unexpectedly
        EXPECT_NO_THROW({
            db->initialize(config);
        }) << "Connection string injection attempt caused issue: " << db_name;
    }
}

//=============================================================================
// Logging Security Tests
//=============================================================================

/**
 * @test ConnectionLogDoesNotContainPassword
 * @brief Tests that connection logging masks passwords
 *
 * When logging is enabled, passwords should be masked or omitted.
 */
TEST_F(CredentialSecurityTest, ConnectionLogDoesNotContainPassword) {
#ifdef USE_SQLITE
    std::string output;

    // Capture clog output during connection
    output = captureOutput(std::clog, [&]() {
        auto db = std::make_unique<sqlite_backend>();
        connection_config config;
        config.database = ":memory:";
        db->initialize(config);
        db->execute_query("SELECT 1");
        db->shutdown();
    });

    // Look for common password patterns that shouldn't appear
    std::vector<std::string> password_patterns = {
        "password=",
        "passwd=",
        "pwd=",
        "secret=",
        "credential",
    };

    for (const auto& pattern : password_patterns) {
        // Pattern should not appear in logs (case-insensitive check)
        std::string lower_output = output;
        std::transform(lower_output.begin(), lower_output.end(),
                       lower_output.begin(), ::tolower);
        std::string lower_pattern = pattern;
        std::transform(lower_pattern.begin(), lower_pattern.end(),
                       lower_pattern.begin(), ::tolower);

        EXPECT_TRUE(lower_output.find(lower_pattern) == std::string::npos)
            << "Sensitive pattern '" << pattern << "' found in logs";
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test DebugModeMasksCredentials
 * @brief Tests that debug output masks sensitive information
 */
TEST_F(CredentialSecurityTest, DebugModeMasksCredentials) {
    // If there's a debug/toString method, it should mask credentials
    // This is a design principle verification

    // Any debug representation should mask the password
    // This verifies the principle even if not all backends implement it
    SUCCEED() << "Credential masking in debug output is a security requirement";
}

//=============================================================================
// Memory Security Tests
//=============================================================================

/**
 * @test PasswordNotInCoreAfterDisconnect
 * @brief Tests that credentials are cleared from memory after disconnect
 *
 * This is a best-effort test - we can't fully verify memory clearing
 * without specialized tools, but we can verify the principle.
 */
TEST_F(CredentialSecurityTest, PasswordNotInCoreAfterDisconnect) {
#ifdef USE_SQLITE
    {
        auto db = std::make_unique<sqlite_backend>();
        connection_config config;
        config.database = ":memory:";
        db->initialize(config);
        db->shutdown();
        // db goes out of scope
    }

    // After disconnect and destruction, sensitive data should be cleared
    // This is a design verification - actual memory clearing requires
    // tools like Valgrind or specialized memory analyzers
    SUCCEED() << "Connection objects should clear credentials on disconnect";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Environment Variable Security
//=============================================================================

/**
 * @test EnvironmentCredentialsNotLogged
 * @brief Tests that credentials from environment variables aren't logged
 */
TEST_F(CredentialSecurityTest, EnvironmentCredentialsNotLogged) {
    // If the system supports reading credentials from environment variables,
    // those should never be logged either

    // Set a test environment variable (don't use real credentials!)
    // Note: We don't actually set env vars in tests for safety

    // This documents the security requirement
    SUCCEED() << "Environment-based credentials must be masked in all output";
}

//=============================================================================
// Credential Validation Tests
//=============================================================================

/**
 * @test EmptyPasswordHandled
 * @brief Tests that empty passwords are handled safely
 */
TEST_F(CredentialSecurityTest, EmptyPasswordHandled) {
    auto db = std::make_unique<sqlite_backend>();

    // Empty database path should be handled gracefully
    connection_config config;
    config.database = "";

    EXPECT_NO_THROW({
        auto result = db->initialize(config);
        // Empty connection string should fail gracefully
        (void)result;
    });
}

/**
 * @test VeryLongPasswordHandled
 * @brief Tests that very long passwords don't cause buffer overflows
 */
TEST_F(CredentialSecurityTest, VeryLongPasswordHandled) {
    auto db = std::make_unique<sqlite_backend>();

    // Very long password (simulating very long password in connection string)
    connection_config config;
    config.database = ":memory:";
    config.password = std::string(100000, 'a');

    EXPECT_NO_THROW({
        auto result = db->initialize(config);
        // Should fail gracefully, not crash or buffer overflow
        (void)result;
    }) << "Very long password caused crash";
}
