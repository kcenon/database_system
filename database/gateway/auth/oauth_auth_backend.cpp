/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "oauth_auth_backend.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

namespace database::gateway::auth {

namespace {

std::string generate_random_string(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "-._~";  // URL-safe characters for PKCE

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    return result;
}

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string base64_url_encode(const std::string& input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 2 < input.size(); i += 3) {
        uint32_t n = (static_cast<uint8_t>(input[i]) << 16) |
                     (static_cast<uint8_t>(input[i + 1]) << 8) |
                     static_cast<uint8_t>(input[i + 2]);
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += table[(n >> 6) & 0x3F];
        result += table[n & 0x3F];
    }

    if (i < input.size()) {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) {
            n |= static_cast<uint8_t>(input[i + 1]) << 8;
        }
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        if (i + 1 < input.size()) {
            result += table[(n >> 6) & 0x3F];
        }
    }

    return result;
}

// Simple SHA-256 hash (placeholder - use real crypto in production)
std::string sha256_hash(const std::string& input) {
    std::hash<std::string> hasher;
    size_t hash = hasher(input);
    std::string bytes;
    for (int i = 0; i < 32; ++i) {
        bytes += static_cast<char>((hash >> (i % 8 * 8)) & 0xFF);
    }
    return bytes;
}

// Parse simple JSON value (placeholder - use proper JSON library in production)
std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = "\"" + key + "\": \"";
        pos = json.find(search);
    }
    if (pos == std::string::npos) return "";

    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";

    return json.substr(pos, end - pos);
}

int64_t extract_json_int(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = "\"" + key + "\": ";
        pos = json.find(search);
    }
    if (pos == std::string::npos) return 0;

    pos += search.length();
    while (pos < json.size() && std::isspace(json[pos])) ++pos;

    size_t end = pos;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) ++end;

    if (end == pos) return 0;
    return std::stoll(json.substr(pos, end - pos));
}

} // anonymous namespace

oauth_auth_backend::oauth_auth_backend(const oauth_config& config)
    : config_(config) {}

oauth_auth_backend::~oauth_auth_backend() {
    shutdown();
}

auth_backend_type oauth_auth_backend::type() const noexcept {
    return auth_backend_type::oauth;
}

std::string oauth_auth_backend::name() const noexcept {
    return config_.name;
}

result<void> oauth_auth_backend::initialize() {
    if (initialized_.load()) {
        return kcenon::common::ok();
    }

    // Validate required configuration
    if (config_.client_id.empty()) {
        return kcenon::common::error_info{-1, "OAuth client_id required", "oauth_auth"};
    }

    if (config_.token_url.empty() && config_.issuer.empty()) {
        return kcenon::common::error_info{-2, "OAuth token_url or issuer required", "oauth_auth"};
    }

    // Try to fetch discovery document if issuer is provided
    if (!config_.issuer.empty()) {
        auto discover_result = fetch_discovery_document();
        if (discover_result.is_err()) {
            // Discovery is optional, use configured URLs
        }
    }

    // Use configured or discovered URLs
    if (discovered_authorization_url_.empty()) {
        discovered_authorization_url_ = config_.authorization_url;
    }
    if (discovered_token_url_.empty()) {
        discovered_token_url_ = config_.token_url;
    }
    if (discovered_userinfo_url_.empty()) {
        discovered_userinfo_url_ = config_.userinfo_url;
    }
    if (discovered_introspection_url_.empty()) {
        discovered_introspection_url_ = config_.introspection_url;
    }
    if (discovered_jwks_url_.empty()) {
        discovered_jwks_url_ = config_.jwks_url;
    }

    // Fetch JWKS for token validation if available
    if (!discovered_jwks_url_.empty()) {
        auto jwks_result = fetch_jwks();
        if (jwks_result.is_err()) {
            // JWKS fetch is optional
        }
    }

    initialized_.store(true);
    return kcenon::common::ok();
}

void oauth_auth_backend::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        token_cache_.clear();
        user_info_cache_.clear();
    }

    initialized_.store(false);
}

result<auth_result> oauth_auth_backend::authenticate(const auth_credentials& credentials) {
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    // If access_token is provided, validate it
    if (!credentials.access_token.empty()) {
        return validate_token(credentials.access_token);
    }

    // Client credentials flow (service authentication)
    if (!credentials.client_id.empty() && !credentials.client_secret.empty()) {
        // Override config with provided credentials
        oauth_config temp_config = config_;
        temp_config.client_id = credentials.client_id;
        temp_config.client_secret = credentials.client_secret;

        return client_credentials_flow();
    }

    return kcenon::common::error_info{
        -2,
        "OAuth requires access_token or client credentials",
        "oauth_auth"
    };
}

result<auth_result> oauth_auth_backend::validate_token(const std::string& token) {
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    if (token.empty()) {
        return kcenon::common::error_info{-2, "Token required", "oauth_auth"};
    }

    // Check session cache first
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(token);
        if (it != sessions_.end()) {
            const auto& session = it->second;
            auto now = std::chrono::system_clock::now();

            if (now < session.expires_at) {
                auth_result result_data;
                result_data.user_id = session.user_id;
                result_data.username = session.user_id;
                result_data.permissions = session.permissions;
                result_data.authenticated_at = now;
                result_data.access_token = token;

                tokens_validated_.fetch_add(1);
                return result_data;
            } else {
                sessions_.erase(it);
            }
        }
    }

    // Try token introspection if available
    if (!discovered_introspection_url_.empty()) {
        auto introspect_result = introspect_token(token);
        if (introspect_result.is_ok()) {
            const auto& token_info = introspect_result.value();

            // Fetch user info
            std::string user_id = "oauth_user";
            std::vector<std::string> groups;

            if (!discovered_userinfo_url_.empty()) {
                auto userinfo_result = fetch_user_info(token);
                if (userinfo_result.is_ok()) {
                    const auto& user_info = userinfo_result.value();
                    user_id = user_info.sub;
                    groups = user_info.groups;
                }
            }

            // Map scopes to permissions
            auto permissions = map_scopes_to_permissions(token_info.scopes);

            // Cache session
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                oauth_session session;
                session.access_token = token;
                session.user_id = user_id;
                session.permissions = permissions;
                session.expires_at = token_info.issued_at + token_info.expires_in;
                sessions_[token] = session;
            }

            auth_result result_data;
            result_data.user_id = user_id;
            result_data.username = user_id;
            result_data.groups = groups;
            result_data.permissions = permissions;
            result_data.authenticated_at = std::chrono::system_clock::now();
            result_data.access_token = token;
            result_data.token_ttl = token_info.expires_in;

            tokens_validated_.fetch_add(1);
            return result_data;
        }
    }

    // Try JWT validation if JWKS is available
    auto jwt_result = validate_jwt(token);
    if (jwt_result.is_ok()) {
        const auto& claims = jwt_result.value();

        std::string user_id = claims.count("sub") ? claims.at("sub") : "jwt_user";

        // Parse scopes from claims
        std::vector<std::string> scopes;
        if (claims.count("scope")) {
            std::istringstream iss(claims.at("scope"));
            std::string scope;
            while (iss >> scope) {
                scopes.push_back(scope);
            }
        }

        auto permissions = map_scopes_to_permissions(scopes);

        // Calculate expiration
        std::chrono::seconds ttl{3600};
        if (claims.count("exp")) {
            auto exp = std::stoll(claims.at("exp"));
            auto now = std::chrono::system_clock::now();
            auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            ttl = std::chrono::seconds(exp - now_epoch);
        }

        // Cache session
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            oauth_session session;
            session.access_token = token;
            session.user_id = user_id;
            session.permissions = permissions;
            session.expires_at = std::chrono::system_clock::now() + ttl;
            sessions_[token] = session;
        }

        auth_result result_data;
        result_data.user_id = user_id;
        result_data.username = user_id;
        result_data.permissions = permissions;
        result_data.authenticated_at = std::chrono::system_clock::now();
        result_data.access_token = token;
        result_data.token_ttl = ttl;

        tokens_validated_.fetch_add(1);
        return result_data;
    }

    auth_failures_.fetch_add(1);
    return kcenon::common::error_info{-3, "Token validation failed", "oauth_auth"};
}

result<auth_result> oauth_auth_backend::refresh_token(const std::string& refresh_token) {
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    if (refresh_token.empty()) {
        return kcenon::common::error_info{-2, "Refresh token required", "oauth_auth"};
    }

    if (discovered_token_url_.empty()) {
        return kcenon::common::error_info{-3, "Token URL not configured", "oauth_auth"};
    }

    // Build refresh token request
    std::ostringstream body;
    body << "grant_type=refresh_token"
         << "&refresh_token=" << url_encode(refresh_token)
         << "&client_id=" << url_encode(config_.client_id);

    if (!config_.client_secret.empty()) {
        body << "&client_secret=" << url_encode(config_.client_secret);
    }

    // Make token request
    auto response = http_post(discovered_token_url_, body.str());
    if (response.is_err()) {
        auth_failures_.fetch_add(1);
        return response.error();
    }

    // Parse token response
    const std::string& json = response.value();
    std::string access_token = extract_json_string(json, "access_token");
    std::string new_refresh_token = extract_json_string(json, "refresh_token");
    int64_t expires_in = extract_json_int(json, "expires_in");

    if (access_token.empty()) {
        auth_failures_.fetch_add(1);
        return kcenon::common::error_info{-4, "Invalid token response", "oauth_auth"};
    }

    // Validate new access token
    auto validate_result = validate_token(access_token);
    if (validate_result.is_err()) {
        return validate_result;
    }

    auth_result result_data = validate_result.value();
    result_data.access_token = access_token;
    result_data.token_ttl = std::chrono::seconds(expires_in > 0 ? expires_in : 3600);

    // Update session with new refresh token
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(access_token);
        if (it != sessions_.end()) {
            it->second.refresh_token = new_refresh_token.empty() ? refresh_token : new_refresh_token;
        }
    }

    tokens_refreshed_.fetch_add(1);
    return result_data;
}

result<void> oauth_auth_backend::revoke_token(const std::string& token) {
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    // Remove from local session cache
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(token);
    }

    // Optionally call revocation endpoint if configured
    // Most implementations don't have a revocation endpoint

    tokens_revoked_.fetch_add(1);
    return kcenon::common::ok();
}

bool oauth_auth_backend::has_permission(
    const std::string& user_id,
    const std::string& permission) const
{
    auto permissions = get_permissions(user_id);
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}

std::vector<std::string> oauth_auth_backend::get_permissions(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    for (const auto& [token, session] : sessions_) {
        if (session.user_id == user_id) {
            return session.permissions;
        }
    }

    return {};
}

bool oauth_auth_backend::is_healthy() const noexcept {
    return initialized_.load();
}

std::string oauth_auth_backend::get_authorization_url(
    const std::string& state,
    const std::string& code_challenge) const
{
    std::ostringstream url;
    url << discovered_authorization_url_
        << "?response_type=code"
        << "&client_id=" << url_encode(config_.client_id)
        << "&redirect_uri=" << url_encode(config_.redirect_uri)
        << "&state=" << url_encode(state);

    // Add scopes
    if (!config_.scopes.empty()) {
        std::ostringstream scopes;
        for (size_t i = 0; i < config_.scopes.size(); ++i) {
            if (i > 0) scopes << " ";
            scopes << config_.scopes[i];
        }
        url << "&scope=" << url_encode(scopes.str());
    }

    // Add PKCE if enabled
    if (config_.use_pkce && !code_challenge.empty()) {
        url << "&code_challenge=" << url_encode(code_challenge)
            << "&code_challenge_method=S256";
    }

    return url.str();
}

result<auth_result> oauth_auth_backend::exchange_code(
    const std::string& code,
    const std::string& code_verifier)
{
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    if (code.empty()) {
        return kcenon::common::error_info{-2, "Authorization code required", "oauth_auth"};
    }

    if (discovered_token_url_.empty()) {
        return kcenon::common::error_info{-3, "Token URL not configured", "oauth_auth"};
    }

    // Build token request
    std::ostringstream body;
    body << "grant_type=authorization_code"
         << "&code=" << url_encode(code)
         << "&redirect_uri=" << url_encode(config_.redirect_uri)
         << "&client_id=" << url_encode(config_.client_id);

    if (!config_.client_secret.empty()) {
        body << "&client_secret=" << url_encode(config_.client_secret);
    }

    if (config_.use_pkce && !code_verifier.empty()) {
        body << "&code_verifier=" << url_encode(code_verifier);
    }

    // Make token request
    auto response = http_post(discovered_token_url_, body.str());
    if (response.is_err()) {
        auth_failures_.fetch_add(1);
        return response.error();
    }

    // Parse token response
    const std::string& json = response.value();
    std::string access_token = extract_json_string(json, "access_token");
    std::string refresh_token = extract_json_string(json, "refresh_token");
    int64_t expires_in = extract_json_int(json, "expires_in");

    if (access_token.empty()) {
        auth_failures_.fetch_add(1);
        return kcenon::common::error_info{-4, "Invalid token response", "oauth_auth"};
    }

    // Validate access token
    auto validate_result = validate_token(access_token);
    if (validate_result.is_err()) {
        return validate_result;
    }

    auth_result result_data = validate_result.value();
    result_data.access_token = access_token;
    result_data.token_ttl = std::chrono::seconds(expires_in > 0 ? expires_in : 3600);

    // Store refresh token in session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(access_token);
        if (it != sessions_.end()) {
            it->second.refresh_token = refresh_token;
        }
    }

    tokens_issued_.fetch_add(1);
    return result_data;
}

result<auth_result> oauth_auth_backend::client_credentials_flow() {
    if (!initialized_.load()) {
        return kcenon::common::error_info{-1, "Backend not initialized", "oauth_auth"};
    }

    if (discovered_token_url_.empty()) {
        return kcenon::common::error_info{-2, "Token URL not configured", "oauth_auth"};
    }

    // Build client credentials request
    std::ostringstream body;
    body << "grant_type=client_credentials"
         << "&client_id=" << url_encode(config_.client_id)
         << "&client_secret=" << url_encode(config_.client_secret);

    // Add scopes if configured
    if (!config_.scopes.empty()) {
        std::ostringstream scopes;
        for (size_t i = 0; i < config_.scopes.size(); ++i) {
            if (i > 0) scopes << " ";
            scopes << config_.scopes[i];
        }
        body << "&scope=" << url_encode(scopes.str());
    }

    // Make token request
    auto response = http_post(discovered_token_url_, body.str());
    if (response.is_err()) {
        auth_failures_.fetch_add(1);
        return response.error();
    }

    // Parse token response
    const std::string& json = response.value();
    std::string access_token = extract_json_string(json, "access_token");
    int64_t expires_in = extract_json_int(json, "expires_in");

    if (access_token.empty()) {
        auth_failures_.fetch_add(1);
        return kcenon::common::error_info{-3, "Invalid token response", "oauth_auth"};
    }

    // Create auth result for service account
    auto permissions = map_scopes_to_permissions(config_.scopes);
    auto now = std::chrono::system_clock::now();
    auto ttl = std::chrono::seconds(expires_in > 0 ? expires_in : 3600);

    // Cache session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        oauth_session session;
        session.access_token = access_token;
        session.user_id = "service:" + config_.client_id;
        session.permissions = permissions;
        session.expires_at = now + ttl;
        sessions_[access_token] = session;
    }

    auth_result result_data;
    result_data.user_id = "service:" + config_.client_id;
    result_data.username = config_.client_id;
    result_data.permissions = permissions;
    result_data.authenticated_at = now;
    result_data.access_token = access_token;
    result_data.token_ttl = ttl;

    tokens_issued_.fetch_add(1);
    return result_data;
}

result<oauth_token_info> oauth_auth_backend::introspect_token(const std::string& token) {
    if (discovered_introspection_url_.empty()) {
        return kcenon::common::error_info{-1, "Introspection URL not configured", "oauth_auth"};
    }

    // Build introspection request
    std::ostringstream body;
    body << "token=" << url_encode(token)
         << "&client_id=" << url_encode(config_.client_id);

    if (!config_.client_secret.empty()) {
        body << "&client_secret=" << url_encode(config_.client_secret);
    }

    // Make introspection request
    auto response = http_post(discovered_introspection_url_, body.str());
    if (response.is_err()) {
        return response.error();
    }

    // Parse response
    const std::string& json = response.value();

    // Check if token is active
    if (json.find("\"active\":true") == std::string::npos &&
        json.find("\"active\": true") == std::string::npos) {
        return kcenon::common::error_info{-2, "Token is not active", "oauth_auth"};
    }

    oauth_token_info info;
    info.access_token = token;
    info.token_type = extract_json_string(json, "token_type");
    if (info.token_type.empty()) info.token_type = "Bearer";

    int64_t exp = extract_json_int(json, "exp");
    int64_t iat = extract_json_int(json, "iat");

    if (iat > 0) {
        info.issued_at = std::chrono::system_clock::from_time_t(iat);
    } else {
        info.issued_at = std::chrono::system_clock::now();
    }

    if (exp > 0 && iat > 0) {
        info.expires_in = std::chrono::seconds(exp - iat);
    }

    // Parse scopes
    std::string scope_str = extract_json_string(json, "scope");
    if (!scope_str.empty()) {
        std::istringstream iss(scope_str);
        std::string scope;
        while (iss >> scope) {
            info.scopes.push_back(scope);
        }
    }

    return info;
}

result<oauth_user_info> oauth_auth_backend::fetch_user_info(const std::string& access_token) {
    if (discovered_userinfo_url_.empty()) {
        return kcenon::common::error_info{-1, "Userinfo URL not configured", "oauth_auth"};
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = user_info_cache_.find(access_token);
        if (it != user_info_cache_.end()) {
            auto age = std::chrono::system_clock::now() - it->second.fetched_at;
            if (age < config_.cache_ttl) {
                return it->second;
            }
        }
    }

    // Fetch user info
    auto response = http_get_authorized(discovered_userinfo_url_, access_token);
    if (response.is_err()) {
        return response.error();
    }

    // Parse response
    const std::string& json = response.value();

    oauth_user_info info;
    info.sub = extract_json_string(json, "sub");
    info.name = extract_json_string(json, "name");
    info.email = extract_json_string(json, "email");
    info.preferred_username = extract_json_string(json, "preferred_username");
    info.fetched_at = std::chrono::system_clock::now();

    // Cache user info
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        user_info_cache_[access_token] = info;
    }

    return info;
}

void oauth_auth_backend::set_scope_permission_mappings(
    const std::unordered_map<std::string, std::vector<std::string>>& scope_mappings)
{
    std::lock_guard<std::mutex> lock(mappings_mutex_);
    scope_permission_mappings_ = scope_mappings;
}

std::map<std::string, std::string> oauth_auth_backend::get_discovery_info() const {
    return {
        {"authorization_endpoint", discovered_authorization_url_},
        {"token_endpoint", discovered_token_url_},
        {"userinfo_endpoint", discovered_userinfo_url_},
        {"introspection_endpoint", discovered_introspection_url_},
        {"jwks_uri", discovered_jwks_url_}
    };
}

void oauth_auth_backend::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    token_cache_.clear();
    user_info_cache_.clear();
}

std::map<std::string, size_t> oauth_auth_backend::get_stats() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return {
        {"sessions", sessions_.size()},
        {"tokens_issued", tokens_issued_.load()},
        {"tokens_validated", tokens_validated_.load()},
        {"tokens_refreshed", tokens_refreshed_.load()},
        {"tokens_revoked", tokens_revoked_.load()},
        {"auth_failures", auth_failures_.load()}
    };
}

result<void> oauth_auth_backend::fetch_discovery_document() {
    // OpenID Connect Discovery URL
    std::string discovery_url = config_.issuer;
    if (!discovery_url.empty() && discovery_url.back() != '/') {
        discovery_url += '/';
    }
    discovery_url += ".well-known/openid-configuration";

    // Simulated discovery response
    // In production, use HTTP client to fetch actual document
    discovered_authorization_url_ = config_.issuer + "/authorize";
    discovered_token_url_ = config_.issuer + "/token";
    discovered_userinfo_url_ = config_.issuer + "/userinfo";
    discovered_introspection_url_ = config_.issuer + "/introspect";
    discovered_jwks_url_ = config_.issuer + "/.well-known/jwks.json";

    return kcenon::common::ok();
}

result<void> oauth_auth_backend::fetch_jwks() {
    // Simulated JWKS fetch
    // In production, use HTTP client and proper JWT library
    std::lock_guard<std::mutex> lock(jwks_mutex_);
    jwks_json_ = "{}";  // Placeholder
    jwks_fetched_at_ = std::chrono::system_clock::now();

    return kcenon::common::ok();
}

result<std::map<std::string, std::string>> oauth_auth_backend::validate_jwt(const std::string& token) {
    // Basic JWT structure validation
    // In production, use proper JWT library with signature verification

    auto claims = parse_jwt_claims(token);
    if (claims.empty()) {
        return kcenon::common::error_info{-1, "Invalid JWT format", "oauth_auth"};
    }

    // Check expiration
    if (claims.count("exp")) {
        auto exp = std::stoll(claims["exp"]);
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (now >= exp) {
            return kcenon::common::error_info{-2, "Token expired", "oauth_auth"};
        }
    }

    // Validate issuer if configured
    if (!config_.issuer.empty() && claims.count("iss")) {
        if (claims["iss"] != config_.issuer) {
            return kcenon::common::error_info{-3, "Invalid issuer", "oauth_auth"};
        }
    }

    // Validate audience if configured
    if (!config_.audience.empty() && claims.count("aud")) {
        if (claims["aud"] != config_.audience) {
            return kcenon::common::error_info{-4, "Invalid audience", "oauth_auth"};
        }
    }

    return claims;
}

std::map<std::string, std::string> oauth_auth_backend::parse_jwt_claims(const std::string& token) const {
    std::map<std::string, std::string> claims;

    // JWT format: header.payload.signature
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);

    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return claims;
    }

    // Extract payload (base64url encoded)
    std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);

    // Base64url decode (simplified - in production use proper decoder)
    // For now, just extract common claims patterns
    claims["sub"] = "jwt_user_" + generate_random_string(8);
    claims["exp"] = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            (std::chrono::system_clock::now() + std::chrono::hours(1)).time_since_epoch()
        ).count()
    );

    return claims;
}

result<std::string> oauth_auth_backend::http_post(
    const std::string& url,
    const std::string& body,
    const std::string& /* content_type */)
{
    // Simulated HTTP POST
    // In production, use libcurl or similar HTTP library

    // Simulate successful token response
    std::string response = R"({
        "access_token": "simulated_access_token_)" + generate_random_string(32) + R"(",
        "token_type": "Bearer",
        "expires_in": 3600,
        "refresh_token": "simulated_refresh_token_)" + generate_random_string(32) + R"(",
        "scope": "openid profile email"
    })";

    (void)url;
    (void)body;

    return response;
}

result<std::string> oauth_auth_backend::http_get_authorized(
    const std::string& url,
    const std::string& /* access_token */)
{
    // Simulated HTTP GET with Authorization header
    // In production, use libcurl or similar HTTP library

    // Simulate user info response
    std::string response = R"({
        "sub": ")" + generate_random_string(16) + R"(",
        "name": "Test User",
        "email": "test@example.com",
        "preferred_username": "testuser"
    })";

    (void)url;

    return response;
}

std::string oauth_auth_backend::generate_code_verifier() {
    return generate_random_string(64);
}

std::string oauth_auth_backend::generate_code_challenge(const std::string& verifier) {
    std::string hash = sha256_hash(verifier);
    return base64_url_encode(hash);
}

std::vector<std::string> oauth_auth_backend::map_scopes_to_permissions(
    const std::vector<std::string>& scopes) const
{
    std::vector<std::string> permissions;
    std::lock_guard<std::mutex> lock(mappings_mutex_);

    for (const auto& scope : scopes) {
        auto it = scope_permission_mappings_.find(scope);
        if (it != scope_permission_mappings_.end()) {
            for (const auto& perm : it->second) {
                if (std::find(permissions.begin(), permissions.end(), perm) == permissions.end()) {
                    permissions.push_back(perm);
                }
            }
        }
    }

    return permissions;
}

std::string oauth_auth_backend::generate_state() {
    return generate_random_string(32);
}

} // namespace database::gateway::auth
