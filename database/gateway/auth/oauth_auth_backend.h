/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include "auth_backend.h"

#include <mutex>
#include <unordered_map>
#include <atomic>

namespace database::gateway::auth {

/**
 * @brief OAuth token information
 */
struct oauth_token_info {
    std::string access_token;
    std::string refresh_token;
    std::string token_type{"Bearer"};
    std::chrono::seconds expires_in{3600};
    std::chrono::system_clock::time_point issued_at;
    std::vector<std::string> scopes;
};

/**
 * @brief OAuth user information from userinfo endpoint
 */
struct oauth_user_info {
    std::string sub;                            ///< Subject (unique user ID)
    std::string name;
    std::string email;
    std::string preferred_username;
    std::vector<std::string> groups;
    std::chrono::system_clock::time_point fetched_at;
};

/**
 * @brief OAuth authentication backend implementation
 *
 * Provides OAuth 2.0 / OpenID Connect authentication with:
 * - Authorization Code flow with PKCE
 * - Client Credentials flow
 * - Token introspection
 * - Token refresh
 * - JWT validation
 * - User info endpoint integration
 * - Scope-based permissions
 *
 * Supported providers:
 * - Generic OAuth 2.0 / OpenID Connect
 * - Okta
 * - Auth0
 * - Keycloak
 * - Azure AD
 * - Google
 *
 * Note: This implementation uses simulated HTTP client for portability.
 * In production, integrate with libcurl or similar HTTP library.
 */
class oauth_auth_backend : public auth_backend_interface {
public:
    /**
     * @brief Construct OAuth auth backend with configuration
     * @param config OAuth configuration
     */
    explicit oauth_auth_backend(const oauth_config& config);

    ~oauth_auth_backend() override;

    // auth_backend_interface implementation
    [[nodiscard]] auth_backend_type type() const noexcept override;
    [[nodiscard]] std::string name() const noexcept override;
    result<void> initialize() override;
    void shutdown() override;
    result<auth_result> authenticate(const auth_credentials& credentials) override;
    result<auth_result> validate_token(const std::string& token) override;
    result<auth_result> refresh_token(const std::string& refresh_token) override;
    result<void> revoke_token(const std::string& token) override;
    [[nodiscard]] bool has_permission(
        const std::string& user_id,
        const std::string& permission) const override;
    [[nodiscard]] std::vector<std::string> get_permissions(
        const std::string& user_id) const override;
    [[nodiscard]] bool is_healthy() const noexcept override;

    // OAuth-specific methods

    /**
     * @brief Generate authorization URL for OAuth flow
     * @param state State parameter for CSRF protection
     * @param code_challenge PKCE code challenge (optional)
     * @return Authorization URL
     */
    std::string get_authorization_url(
        const std::string& state,
        const std::string& code_challenge = "") const;

    /**
     * @brief Exchange authorization code for tokens
     * @param code Authorization code from callback
     * @param code_verifier PKCE code verifier (if PKCE was used)
     * @return Authentication result with tokens
     */
    result<auth_result> exchange_code(
        const std::string& code,
        const std::string& code_verifier = "");

    /**
     * @brief Authenticate using client credentials flow
     * @return Authentication result for service account
     */
    result<auth_result> client_credentials_flow();

    /**
     * @brief Introspect an access token
     * @param token Access token to introspect
     * @return Token information if active, error otherwise
     */
    result<oauth_token_info> introspect_token(const std::string& token);

    /**
     * @brief Fetch user info from userinfo endpoint
     * @param access_token Valid access token
     * @return User information
     */
    result<oauth_user_info> fetch_user_info(const std::string& access_token);

    /**
     * @brief Map OAuth scopes to permissions
     * @param scope_mappings Map of OAuth scope to permissions list
     */
    void set_scope_permission_mappings(
        const std::unordered_map<std::string, std::vector<std::string>>& scope_mappings);

    /**
     * @brief Get provider discovery information
     * @return Discovery metadata as key-value pairs
     */
    [[nodiscard]] std::map<std::string, std::string> get_discovery_info() const;

    /**
     * @brief Clear token cache
     */
    void clear_cache();

    /**
     * @brief Get statistics
     * @return Map of statistics
     */
    [[nodiscard]] std::map<std::string, size_t> get_stats() const;

private:
    /**
     * @brief Fetch OAuth discovery document
     */
    result<void> fetch_discovery_document();

    /**
     * @brief Fetch JWKS for token validation
     */
    result<void> fetch_jwks();

    /**
     * @brief Validate JWT token
     * @param token JWT token
     * @return Parsed claims if valid
     */
    result<std::map<std::string, std::string>> validate_jwt(const std::string& token);

    /**
     * @brief Parse JWT without validation
     * @param token JWT token
     * @return Parsed claims
     */
    std::map<std::string, std::string> parse_jwt_claims(const std::string& token) const;

    /**
     * @brief Make HTTP POST request
     * @param url Request URL
     * @param body Request body
     * @param content_type Content type
     * @return Response body or error
     */
    result<std::string> http_post(
        const std::string& url,
        const std::string& body,
        const std::string& content_type = "application/x-www-form-urlencoded");

    /**
     * @brief Make HTTP GET request with authorization
     * @param url Request URL
     * @param access_token Bearer token
     * @return Response body or error
     */
    result<std::string> http_get_authorized(
        const std::string& url,
        const std::string& access_token);

    /**
     * @brief Generate PKCE code verifier
     */
    static std::string generate_code_verifier();

    /**
     * @brief Generate PKCE code challenge from verifier
     */
    static std::string generate_code_challenge(const std::string& verifier);

    /**
     * @brief Map scopes to permissions
     */
    std::vector<std::string> map_scopes_to_permissions(
        const std::vector<std::string>& scopes) const;

    /**
     * @brief Generate random state parameter
     */
    static std::string generate_state();

    oauth_config config_;
    std::atomic<bool> initialized_{false};

    // Discovery document endpoints (if OIDC)
    std::string discovered_authorization_url_;
    std::string discovered_token_url_;
    std::string discovered_userinfo_url_;
    std::string discovered_introspection_url_;
    std::string discovered_jwks_url_;

    // JWKS for token validation
    mutable std::mutex jwks_mutex_;
    std::string jwks_json_;
    std::chrono::system_clock::time_point jwks_fetched_at_;

    // Token cache (for validated tokens)
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, oauth_token_info> token_cache_;
    std::unordered_map<std::string, oauth_user_info> user_info_cache_;

    // Active sessions
    mutable std::mutex sessions_mutex_;
    struct oauth_session {
        std::string access_token;
        std::string refresh_token;
        std::string user_id;
        std::vector<std::string> permissions;
        std::chrono::system_clock::time_point expires_at;
    };
    std::unordered_map<std::string, oauth_session> sessions_;

    // Scope to permission mappings
    mutable std::mutex mappings_mutex_;
    std::unordered_map<std::string, std::vector<std::string>> scope_permission_mappings_;

    // Statistics
    std::atomic<size_t> tokens_issued_{0};
    std::atomic<size_t> tokens_validated_{0};
    std::atomic<size_t> tokens_refreshed_{0};
    std::atomic<size_t> tokens_revoked_{0};
    std::atomic<size_t> auth_failures_{0};
};

} // namespace database::gateway::auth
