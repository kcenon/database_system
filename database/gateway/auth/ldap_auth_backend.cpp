/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "ldap_auth_backend.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <regex>

namespace database::gateway::auth {

namespace {

std::string generate_random_string(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

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

// Parse LDAP URL to extract host and port
std::pair<std::string, uint16_t> parse_ldap_url(const std::string& url) {
    std::regex url_regex(R"(ldaps?://([^:/]+)(?::(\d+))?)");
    std::smatch match;

    if (std::regex_match(url, match, url_regex)) {
        std::string host = match[1].str();
        uint16_t port = 389;  // Default LDAP port
        if (match[2].matched) {
            port = static_cast<uint16_t>(std::stoi(match[2].str()));
        } else if (url.substr(0, 5) == "ldaps") {
            port = 636;  // Default LDAPS port
        }
        return {host, port};
    }

    return {"", 0};
}

// Format LDAP search filter by replacing {0} with the value
std::string format_filter(const std::string& filter, const std::string& value) {
    std::string result = filter;
    size_t pos = result.find("{0}");
    if (pos != std::string::npos) {
        result.replace(pos, 3, value);
    }
    return result;
}

} // anonymous namespace

ldap_auth_backend::ldap_auth_backend(const ldap_config& config)
    : config_(config) {}

ldap_auth_backend::~ldap_auth_backend() {
    shutdown();
}

auth_backend_type ldap_auth_backend::type() const noexcept {
    return auth_backend_type::ldap;
}

std::string ldap_auth_backend::name() const noexcept {
    return config_.name;
}

result<void> ldap_auth_backend::initialize() {
    if (initialized_.load()) {
        return result<void>::ok();
    }

    // Validate configuration
    if (config_.server_url.empty()) {
        return result<void>(error_info{-1, "LDAP server URL required", "ldap_auth"});
    }

    if (config_.base_dn.empty()) {
        return result<void>(error_info{-2, "LDAP base DN required", "ldap_auth"});
    }

    // Parse and validate server URL
    auto [host, port] = parse_ldap_url(config_.server_url);
    if (host.empty()) {
        return result<void>(error_info{-3, "Invalid LDAP server URL", "ldap_auth"});
    }

    // Initialize connection
    auto connect_result = connect();
    if (connect_result.is_err()) {
        return connect_result;
    }

    // Bind with service account if configured
    if (!config_.bind_dn.empty()) {
        auto bind_result = bind_service_account();
        if (bind_result.is_err()) {
            disconnect();
            return bind_result;
        }
    }

    initialized_.store(true);
    return result<void>::ok();
}

void ldap_auth_backend::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    disconnect();

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        user_cache_.clear();
    }

    initialized_.store(false);
}

result<auth_result> ldap_auth_backend::authenticate(const auth_credentials& credentials) {
    if (!initialized_.load()) {
        return result<auth_result>(error_info{-1, "Backend not initialized", "ldap_auth"});
    }

    if (credentials.username.empty() || credentials.password.empty()) {
        return result<auth_result>(error_info{-2, "Username and password required", "ldap_auth"});
    }

    // Check cache first
    auto cached = get_cached_user(credentials.username);
    if (cached.has_value()) {
        cache_hits_.fetch_add(1);
        // Still need to verify password by binding
    } else {
        cache_misses_.fetch_add(1);
    }

    // Search for user DN
    auto search_result = search_user_dn(credentials.username);
    if (search_result.is_err()) {
        auth_failures_.fetch_add(1);
        return result<auth_result>(search_result.error());
    }

    std::string user_dn = search_result.value();

    // Bind as user to verify password
    auto bind_result = bind_user(user_dn, credentials.password);
    if (bind_result.is_err()) {
        auth_failures_.fetch_add(1);
        return result<auth_result>(error_info{-4, "Invalid credentials", "ldap_auth"});
    }

    // Fetch user groups
    auto groups_result = fetch_user_groups(user_dn);
    std::vector<std::string> groups;
    if (groups_result.is_ok()) {
        groups = groups_result.value();
    }

    // Map groups to permissions
    auto permissions = map_groups_to_permissions(groups);

    // Generate session token
    std::string token = generate_token();
    auto now = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        ldap_session session;
        session.token = token;
        session.username = credentials.username;
        session.expires_at = now + config_.cache_ttl;
        sessions_[token] = session;
    }

    // Cache user info
    ldap_cached_user cached_user;
    cached_user.user_id = "ldap_" + credentials.username;
    cached_user.username = credentials.username;
    cached_user.dn = user_dn;
    cached_user.groups = groups;
    cached_user.permissions = permissions;
    cached_user.cached_at = now;
    cache_user(cached_user);

    auth_result result_data;
    result_data.user_id = cached_user.user_id;
    result_data.username = credentials.username;
    result_data.groups = groups;
    result_data.permissions = permissions;
    result_data.authenticated_at = now;
    result_data.token_ttl = config_.cache_ttl;
    result_data.access_token = token;

    auth_successes_.fetch_add(1);
    return result<auth_result>::ok(std::move(result_data));
}

result<auth_result> ldap_auth_backend::validate_token(const std::string& token) {
    if (!initialized_.load()) {
        return result<auth_result>(error_info{-1, "Backend not initialized", "ldap_auth"});
    }

    if (token.empty()) {
        return result<auth_result>(error_info{-2, "Token required", "ldap_auth"});
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return result<auth_result>(error_info{-3, "Token not found", "ldap_auth"});
    }

    const auto& session = it->second;
    auto now = std::chrono::system_clock::now();

    if (now > session.expires_at) {
        sessions_.erase(it);
        return result<auth_result>(error_info{-4, "Token expired", "ldap_auth"});
    }

    // Get cached user info
    auto cached = get_cached_user(session.username);
    if (!cached.has_value()) {
        return result<auth_result>(error_info{-5, "User info not found in cache", "ldap_auth"});
    }

    auth_result result_data;
    result_data.user_id = cached->user_id;
    result_data.username = cached->username;
    result_data.groups = cached->groups;
    result_data.permissions = cached->permissions;
    result_data.authenticated_at = cached->cached_at;
    result_data.access_token = token;

    return result<auth_result>::ok(std::move(result_data));
}

result<auth_result> ldap_auth_backend::refresh_token(const std::string& /* refresh_token */) {
    // LDAP doesn't use refresh tokens
    return result<auth_result>(error_info{-10, "Refresh tokens not supported for LDAP auth", "ldap_auth"});
}

result<void> ldap_auth_backend::revoke_token(const std::string& token) {
    if (!initialized_.load()) {
        return result<void>(error_info{-1, "Backend not initialized", "ldap_auth"});
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return result<void>(error_info{-2, "Token not found", "ldap_auth"});
    }

    sessions_.erase(it);
    return result<void>::ok();
}

bool ldap_auth_backend::has_permission(
    const std::string& user_id,
    const std::string& permission) const
{
    auto permissions = get_permissions(user_id);
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}

std::vector<std::string> ldap_auth_backend::get_permissions(const std::string& user_id) const {
    // Extract username from user_id (format: ldap_username)
    std::string username = user_id;
    if (username.substr(0, 5) == "ldap_") {
        username = username.substr(5);
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = user_cache_.find(username);
    if (it != user_cache_.end()) {
        return it->second.permissions;
    }

    return {};
}

bool ldap_auth_backend::is_healthy() const noexcept {
    return initialized_.load() &&
           connection_state_.load() == ldap_connection_state::connected;
}

result<void> ldap_auth_backend::test_connection() {
    if (!initialized_.load()) {
        auto init_result = initialize();
        if (init_result.is_err()) {
            return init_result;
        }
    }

    return connection_state_.load() == ldap_connection_state::connected
        ? result<void>::ok()
        : result<void>(error_info{-1, "LDAP connection not established", "ldap_auth"});
}

ldap_connection_state ldap_auth_backend::connection_state() const noexcept {
    return connection_state_.load();
}

void ldap_auth_backend::set_group_permission_mappings(
    const std::unordered_map<std::string, std::vector<std::string>>& group_mappings)
{
    std::lock_guard<std::mutex> lock(mappings_mutex_);
    group_permission_mappings_ = group_mappings;
}

void ldap_auth_backend::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    user_cache_.clear();
    cache_hits_.store(0);
    cache_misses_.store(0);
}

std::map<std::string, size_t> ldap_auth_backend::get_cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return {
        {"cache_size", user_cache_.size()},
        {"cache_hits", cache_hits_.load()},
        {"cache_misses", cache_misses_.load()},
        {"auth_successes", auth_successes_.load()},
        {"auth_failures", auth_failures_.load()}
    };
}

result<void> ldap_auth_backend::connect() {
    connection_state_.store(ldap_connection_state::connecting);

    // Simulated LDAP connection
    // In production, use ldap_initialize() from libldap:
    //
    // LDAP* ld = nullptr;
    // int rc = ldap_initialize(&ld, config_.server_url.c_str());
    // if (rc != LDAP_SUCCESS) {
    //     connection_state_.store(ldap_connection_state::error);
    //     return result<void>(error_info{rc, ldap_err2string(rc), "ldap_auth"});
    // }
    //
    // if (config_.use_tls) {
    //     rc = ldap_start_tls_s(ld, nullptr, nullptr);
    //     if (rc != LDAP_SUCCESS) {
    //         ldap_unbind_ext_s(ld, nullptr, nullptr);
    //         return result<void>(error_info{rc, "TLS handshake failed", "ldap_auth"});
    //     }
    // }

    // Simulated successful connection
    ldap_handle_ = reinterpret_cast<void*>(0x1);  // Non-null marker
    connection_state_.store(ldap_connection_state::connected);

    return result<void>::ok();
}

void ldap_auth_backend::disconnect() {
    if (ldap_handle_ != nullptr) {
        // In production:
        // ldap_unbind_ext_s(static_cast<LDAP*>(ldap_handle_), nullptr, nullptr);
        ldap_handle_ = nullptr;
    }
    connection_state_.store(ldap_connection_state::disconnected);
}

result<void> ldap_auth_backend::bind_service_account() {
    if (ldap_handle_ == nullptr) {
        return result<void>(error_info{-1, "Not connected to LDAP server", "ldap_auth"});
    }

    // In production:
    // struct berval cred;
    // cred.bv_val = const_cast<char*>(config_.bind_password.c_str());
    // cred.bv_len = config_.bind_password.length();
    //
    // int rc = ldap_sasl_bind_s(
    //     static_cast<LDAP*>(ldap_handle_),
    //     config_.bind_dn.c_str(),
    //     LDAP_SASL_SIMPLE,
    //     &cred,
    //     nullptr, nullptr, nullptr);
    //
    // if (rc != LDAP_SUCCESS) {
    //     return result<void>(error_info{rc, "Service account bind failed", "ldap_auth"});
    // }

    return result<void>::ok();
}

result<std::string> ldap_auth_backend::search_user_dn(const std::string& username) {
    if (ldap_handle_ == nullptr) {
        return result<std::string>(error_info{-1, "Not connected to LDAP server", "ldap_auth"});
    }

    // Format search filter
    std::string filter = format_filter(config_.user_search_filter, username);

    // In production:
    // LDAPMessage* result = nullptr;
    // int rc = ldap_search_ext_s(
    //     static_cast<LDAP*>(ldap_handle_),
    //     config_.base_dn.c_str(),
    //     LDAP_SCOPE_SUBTREE,
    //     filter.c_str(),
    //     nullptr,  // attrs
    //     0,        // attrsonly
    //     nullptr,  // serverctrls
    //     nullptr,  // clientctrls
    //     nullptr,  // timeout
    //     0,        // sizelimit
    //     &result);
    //
    // if (rc != LDAP_SUCCESS) {
    //     return result<std::string>(error_info{rc, "User search failed", "ldap_auth"});
    // }
    //
    // LDAPMessage* entry = ldap_first_entry(static_cast<LDAP*>(ldap_handle_), result);
    // if (entry == nullptr) {
    //     ldap_msgfree(result);
    //     return result<std::string>(error_info{-2, "User not found", "ldap_auth"});
    // }
    //
    // char* dn = ldap_get_dn(static_cast<LDAP*>(ldap_handle_), entry);
    // std::string user_dn(dn);
    // ldap_memfree(dn);
    // ldap_msgfree(result);

    // Simulated: return a constructed DN
    std::string user_dn = config_.user_id_attribute + "=" + username + "," + config_.base_dn;
    return result<std::string>::ok(user_dn);
}

result<void> ldap_auth_backend::bind_user(const std::string& user_dn, const std::string& password) {
    if (ldap_handle_ == nullptr) {
        return result<void>(error_info{-1, "Not connected to LDAP server", "ldap_auth"});
    }

    // In production:
    // Create a new LDAP connection for user bind to avoid affecting service connection
    // LDAP* user_ld = nullptr;
    // int rc = ldap_initialize(&user_ld, config_.server_url.c_str());
    // if (rc != LDAP_SUCCESS) {
    //     return result<void>(error_info{rc, "Failed to create user connection", "ldap_auth"});
    // }
    //
    // struct berval cred;
    // cred.bv_val = const_cast<char*>(password.c_str());
    // cred.bv_len = password.length();
    //
    // rc = ldap_sasl_bind_s(user_ld, user_dn.c_str(), LDAP_SASL_SIMPLE,
    //                       &cred, nullptr, nullptr, nullptr);
    // ldap_unbind_ext_s(user_ld, nullptr, nullptr);
    //
    // if (rc != LDAP_SUCCESS) {
    //     return result<void>(error_info{rc, "User bind failed", "ldap_auth"});
    // }

    // Simulated: accept any non-empty password for testing
    if (password.empty()) {
        return result<void>(error_info{-2, "Password required", "ldap_auth"});
    }

    return result<void>::ok();
}

result<std::vector<std::string>> ldap_auth_backend::fetch_user_groups(const std::string& user_dn) {
    if (ldap_handle_ == nullptr) {
        return result<std::vector<std::string>>(
            error_info{-1, "Not connected to LDAP server", "ldap_auth"});
    }

    std::string group_base = config_.group_base_dn.empty()
        ? config_.base_dn
        : config_.group_base_dn;

    std::string filter = format_filter(config_.group_search_filter, user_dn);

    // In production:
    // LDAPMessage* result = nullptr;
    // const char* attrs[] = {config_.group_attribute.c_str(), nullptr};
    // int rc = ldap_search_ext_s(...);
    //
    // Parse results to extract group names

    // Simulated: return empty groups
    return result<std::vector<std::string>>::ok(std::vector<std::string>{});
}

std::vector<std::string> ldap_auth_backend::map_groups_to_permissions(
    const std::vector<std::string>& groups) const
{
    std::vector<std::string> permissions;
    std::lock_guard<std::mutex> lock(mappings_mutex_);

    for (const auto& group : groups) {
        auto it = group_permission_mappings_.find(group);
        if (it != group_permission_mappings_.end()) {
            for (const auto& perm : it->second) {
                if (std::find(permissions.begin(), permissions.end(), perm) == permissions.end()) {
                    permissions.push_back(perm);
                }
            }
        }
    }

    return permissions;
}

std::string ldap_auth_backend::generate_token() const {
    return "ldap_tok_" + generate_random_string(32);
}

std::optional<ldap_cached_user> ldap_auth_backend::get_cached_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = user_cache_.find(username);
    if (it == user_cache_.end()) {
        return std::nullopt;
    }

    // Check if cache entry is still valid
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.cached_at);

    if (age > config_.cache_ttl) {
        user_cache_.erase(it);
        return std::nullopt;
    }

    return it->second;
}

void ldap_auth_backend::cache_user(const ldap_cached_user& user) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    user_cache_[user.username] = user;
}

} // namespace database::gateway::auth
