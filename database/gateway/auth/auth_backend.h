/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>
#include "../../core/result.h"

namespace database::gateway::auth {

/**
 * @brief Authentication result containing user information
 */
struct auth_result {
    std::string user_id;                        ///< Authenticated user identifier
    std::string username;                       ///< Username
    std::vector<std::string> groups;            ///< User's group memberships
    std::vector<std::string> permissions;       ///< User's permissions
    std::chrono::system_clock::time_point authenticated_at;  ///< Authentication timestamp
    std::optional<std::chrono::seconds> token_ttl;  ///< Token time-to-live (for OAuth)
    std::string access_token;                   ///< Access token (for OAuth)
};

/**
 * @brief Authentication credentials
 */
struct auth_credentials {
    std::string username;                       ///< Username
    std::string password;                       ///< Password (for local/LDAP)
    std::string access_token;                   ///< Access token (for OAuth)
    std::string refresh_token;                  ///< Refresh token (for OAuth)
    std::string client_id;                      ///< Client ID (for OAuth)
    std::string client_secret;                  ///< Client secret (for OAuth)
};

/**
 * @brief Base configuration for authentication backends
 */
struct auth_backend_config {
    std::string name{"default"};                ///< Backend name for identification
    bool enabled{true};                         ///< Whether this backend is enabled
    std::chrono::seconds cache_ttl{300};        ///< Authentication cache TTL
    size_t max_retries{3};                      ///< Maximum retry attempts
    std::chrono::milliseconds timeout{5000};    ///< Operation timeout
};

/**
 * @brief LDAP-specific configuration
 */
struct ldap_config : auth_backend_config {
    std::string server_url;                     ///< LDAP server URL (ldap://host:389)
    std::string bind_dn;                        ///< Bind DN for LDAP connection
    std::string bind_password;                  ///< Bind password
    std::string base_dn;                        ///< Base DN for user search
    std::string user_search_filter{"(uid={0})"}; ///< User search filter ({0} = username)
    std::string group_search_filter{"(member={0})"}; ///< Group search filter
    std::string group_base_dn;                  ///< Base DN for group search
    std::string user_id_attribute{"uid"};       ///< Attribute for user ID
    std::string group_attribute{"cn"};          ///< Attribute for group name
    bool use_tls{true};                         ///< Use TLS/SSL
    bool verify_certificate{true};              ///< Verify server certificate
    std::string ca_cert_file;                   ///< CA certificate file path
};

/**
 * @brief OAuth-specific configuration
 */
struct oauth_config : auth_backend_config {
    std::string provider_name;                  ///< OAuth provider name
    std::string authorization_url;              ///< Authorization endpoint
    std::string token_url;                      ///< Token endpoint
    std::string userinfo_url;                   ///< User info endpoint
    std::string introspection_url;              ///< Token introspection endpoint
    std::string client_id;                      ///< OAuth client ID
    std::string client_secret;                  ///< OAuth client secret
    std::string redirect_uri;                   ///< Redirect URI
    std::vector<std::string> scopes{"openid", "profile", "email"};  ///< OAuth scopes
    bool use_pkce{true};                        ///< Use PKCE for authorization code flow
    std::string issuer;                         ///< JWT issuer for validation
    std::string audience;                       ///< JWT audience for validation
    std::string jwks_url;                       ///< JWKS URL for token validation
};

/**
 * @brief Local authentication configuration
 */
struct local_config : auth_backend_config {
    std::string password_hash_algorithm{"argon2"}; ///< Hash algorithm (bcrypt, argon2, sha256)
    size_t min_password_length{8};              ///< Minimum password length
    bool require_uppercase{true};               ///< Require uppercase in password
    bool require_lowercase{true};               ///< Require lowercase in password
    bool require_digit{true};                   ///< Require digit in password
    bool require_special{false};                ///< Require special character
    size_t max_failed_attempts{5};              ///< Max failed attempts before lockout
    std::chrono::seconds lockout_duration{900}; ///< Account lockout duration (15 min)
};

/**
 * @brief Authentication backend types
 */
enum class auth_backend_type {
    local,      ///< Local user database
    ldap,       ///< LDAP/Active Directory
    oauth       ///< OAuth 2.0 / OpenID Connect
};

/**
 * @brief Abstract authentication backend interface
 *
 * This interface defines the contract for all authentication backends.
 * Implementations can provide local, LDAP, OAuth, or custom authentication.
 */
class auth_backend_interface {
public:
    virtual ~auth_backend_interface() = default;

    /**
     * @brief Get the backend type
     * @return Authentication backend type
     */
    [[nodiscard]] virtual auth_backend_type type() const noexcept = 0;

    /**
     * @brief Get the backend name
     * @return Backend name
     */
    [[nodiscard]] virtual std::string name() const noexcept = 0;

    /**
     * @brief Initialize the authentication backend
     * @return result::ok() on success, error on failure
     */
    virtual result<void> initialize() = 0;

    /**
     * @brief Shutdown the authentication backend
     */
    virtual void shutdown() = 0;

    /**
     * @brief Authenticate user credentials
     * @param credentials User credentials
     * @return Authentication result on success, error on failure
     */
    virtual result<auth_result> authenticate(const auth_credentials& credentials) = 0;

    /**
     * @brief Validate an existing session/token
     * @param token Session token or access token
     * @return Authentication result if valid, error otherwise
     */
    virtual result<auth_result> validate_token(const std::string& token) = 0;

    /**
     * @brief Refresh an expired token (for OAuth)
     * @param refresh_token Refresh token
     * @return New authentication result on success, error on failure
     */
    virtual result<auth_result> refresh_token(const std::string& refresh_token) = 0;

    /**
     * @brief Revoke a session/token
     * @param token Token to revoke
     * @return result::ok() on success, error on failure
     */
    virtual result<void> revoke_token(const std::string& token) = 0;

    /**
     * @brief Check if user has a specific permission
     * @param user_id User identifier
     * @param permission Permission to check
     * @return true if user has permission
     */
    [[nodiscard]] virtual bool has_permission(
        const std::string& user_id,
        const std::string& permission) const = 0;

    /**
     * @brief Get user's permissions
     * @param user_id User identifier
     * @return List of permissions
     */
    [[nodiscard]] virtual std::vector<std::string> get_permissions(
        const std::string& user_id) const = 0;

    /**
     * @brief Check if the backend is healthy
     * @return true if backend is operational
     */
    [[nodiscard]] virtual bool is_healthy() const noexcept = 0;

protected:
    auth_backend_interface() = default;
    auth_backend_interface(const auth_backend_interface&) = default;
    auth_backend_interface& operator=(const auth_backend_interface&) = default;
    auth_backend_interface(auth_backend_interface&&) = default;
    auth_backend_interface& operator=(auth_backend_interface&&) = default;
};

} // namespace database::gateway::auth
