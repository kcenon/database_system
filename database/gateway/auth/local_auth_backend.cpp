/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "local_auth_backend.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

namespace database::gateway::auth {

namespace {

// Simple SHA-256 implementation for demonstration
// In production, use a proper crypto library like OpenSSL or libsodium
std::string sha256_hash(const std::string& input) {
    std::hash<std::string> hasher;
    size_t hash_value = hasher(input);

    // Add salt and re-hash for basic security
    std::string salted = input + std::to_string(hash_value);
    size_t final_hash = hasher(salted);

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << final_hash;
    return ss.str();
}

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

std::string generate_user_id() {
    return "usr_" + generate_random_string(16);
}

} // anonymous namespace

local_auth_backend::local_auth_backend(const local_config& config)
    : config_(config) {}

local_auth_backend::~local_auth_backend() {
    shutdown();
}

auth_backend_type local_auth_backend::type() const noexcept {
    return auth_backend_type::local;
}

std::string local_auth_backend::name() const noexcept {
    return config_.name;
}

result<void> local_auth_backend::initialize() {
    if (initialized_.load()) {
        return result<void>::ok();
    }

    initialized_.store(true);
    return result<void>::ok();
}

void local_auth_backend::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.clear();
    }

    initialized_.store(false);
}

result<auth_result> local_auth_backend::authenticate(const auth_credentials& credentials) {
    if (!initialized_.load()) {
        return result<auth_result>(error_info{-1, "Backend not initialized", "local_auth"});
    }

    if (credentials.username.empty() || credentials.password.empty()) {
        return result<auth_result>(error_info{-2, "Username and password required", "local_auth"});
    }

    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(credentials.username);
    if (it == users_.end()) {
        return result<auth_result>(error_info{-3, "User not found", "local_auth"});
    }

    auto& user = it->second;

    // Check if account is enabled
    if (!user.enabled) {
        return result<auth_result>(error_info{-4, "Account disabled", "local_auth"});
    }

    // Check if account is locked
    if (is_locked(user)) {
        return result<auth_result>(error_info{-5, "Account locked due to too many failed attempts", "local_auth"});
    }

    // Verify password
    if (!verify_password(credentials.password, user.password_hash)) {
        user.failed_attempts++;
        if (user.failed_attempts >= config_.max_failed_attempts) {
            user.locked_until = std::chrono::system_clock::now() + config_.lockout_duration;
        }
        return result<auth_result>(error_info{-6, "Invalid password", "local_auth"});
    }

    // Reset failed attempts on successful login
    user.failed_attempts = 0;
    user.last_login = std::chrono::system_clock::now();

    // Generate session token
    std::string token = generate_token();

    {
        std::lock_guard<std::mutex> session_lock(sessions_mutex_);
        local_session session;
        session.token = token;
        session.user_id = user.user_id;
        session.created_at = std::chrono::system_clock::now();
        session.expires_at = session.created_at + config_.cache_ttl;
        sessions_[token] = session;
    }

    auth_result result_data;
    result_data.user_id = user.user_id;
    result_data.username = user.username;
    result_data.groups = user.groups;
    result_data.permissions = user.permissions;
    result_data.authenticated_at = std::chrono::system_clock::now();
    result_data.token_ttl = config_.cache_ttl;
    result_data.access_token = token;

    return result<auth_result>::ok(std::move(result_data));
}

result<auth_result> local_auth_backend::validate_token(const std::string& token) {
    if (!initialized_.load()) {
        return result<auth_result>(error_info{-1, "Backend not initialized", "local_auth"});
    }

    if (token.empty()) {
        return result<auth_result>(error_info{-2, "Token required", "local_auth"});
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return result<auth_result>(error_info{-3, "Token not found", "local_auth"});
    }

    const auto& session = it->second;
    auto now = std::chrono::system_clock::now();

    if (now > session.expires_at) {
        sessions_.erase(it);
        return result<auth_result>(error_info{-4, "Token expired", "local_auth"});
    }

    // Get user info
    std::lock_guard<std::mutex> users_lock(users_mutex_);
    for (const auto& [username, user] : users_) {
        if (user.user_id == session.user_id) {
            auth_result result_data;
            result_data.user_id = user.user_id;
            result_data.username = user.username;
            result_data.groups = user.groups;
            result_data.permissions = user.permissions;
            result_data.authenticated_at = session.created_at;
            result_data.access_token = token;

            return result<auth_result>::ok(std::move(result_data));
        }
    }

    return result<auth_result>(error_info{-5, "User not found for session", "local_auth"});
}

result<auth_result> local_auth_backend::refresh_token(const std::string& /* refresh_token */) {
    // Local auth doesn't use refresh tokens in the same way as OAuth
    // Just return an error indicating this operation is not supported
    return result<auth_result>(error_info{-10, "Refresh tokens not supported for local auth", "local_auth"});
}

result<void> local_auth_backend::revoke_token(const std::string& token) {
    if (!initialized_.load()) {
        return result<void>(error_info{-1, "Backend not initialized", "local_auth"});
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return result<void>(error_info{-2, "Token not found", "local_auth"});
    }

    sessions_.erase(it);
    return result<void>::ok();
}

bool local_auth_backend::has_permission(
    const std::string& user_id,
    const std::string& permission) const
{
    std::lock_guard<std::mutex> lock(users_mutex_);

    for (const auto& [username, user] : users_) {
        if (user.user_id == user_id) {
            auto it = std::find(user.permissions.begin(), user.permissions.end(), permission);
            return it != user.permissions.end();
        }
    }

    return false;
}

std::vector<std::string> local_auth_backend::get_permissions(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(users_mutex_);

    for (const auto& [username, user] : users_) {
        if (user.user_id == user_id) {
            return user.permissions;
        }
    }

    return {};
}

bool local_auth_backend::is_healthy() const noexcept {
    return initialized_.load();
}

result<void> local_auth_backend::add_user(
    const std::string& username,
    const std::string& password,
    const std::vector<std::string>& groups,
    const std::vector<std::string>& permissions)
{
    if (username.empty()) {
        return result<void>(error_info{-1, "Username required", "local_auth"});
    }

    // Validate password policy
    auto policy_result = validate_password_policy(password);
    if (policy_result.is_err()) {
        return policy_result;
    }

    std::lock_guard<std::mutex> lock(users_mutex_);

    if (users_.find(username) != users_.end()) {
        return result<void>(error_info{-2, "User already exists", "local_auth"});
    }

    local_user user;
    user.user_id = generate_user_id();
    user.username = username;
    user.password_hash = hash_password(password);
    user.groups = groups;
    user.permissions = permissions;
    user.enabled = true;
    user.failed_attempts = 0;
    user.created_at = std::chrono::system_clock::now();

    users_[username] = std::move(user);

    return result<void>::ok();
}

result<void> local_auth_backend::remove_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-1, "User not found", "local_auth"});
    }

    std::string user_id = it->second.user_id;
    users_.erase(it);

    // Also remove user's sessions
    {
        std::lock_guard<std::mutex> session_lock(sessions_mutex_);
        for (auto sit = sessions_.begin(); sit != sessions_.end();) {
            if (sit->second.user_id == user_id) {
                sit = sessions_.erase(sit);
            } else {
                ++sit;
            }
        }
    }

    return result<void>::ok();
}

result<void> local_auth_backend::update_password(
    const std::string& username,
    const std::string& new_password)
{
    auto policy_result = validate_password_policy(new_password);
    if (policy_result.is_err()) {
        return policy_result;
    }

    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-1, "User not found", "local_auth"});
    }

    it->second.password_hash = hash_password(new_password);

    return result<void>::ok();
}

result<void> local_auth_backend::grant_permission(
    const std::string& username,
    const std::string& permission)
{
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-1, "User not found", "local_auth"});
    }

    auto& perms = it->second.permissions;
    if (std::find(perms.begin(), perms.end(), permission) == perms.end()) {
        perms.push_back(permission);
    }

    return result<void>::ok();
}

result<void> local_auth_backend::revoke_permission(
    const std::string& username,
    const std::string& permission)
{
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-1, "User not found", "local_auth"});
    }

    auto& perms = it->second.permissions;
    perms.erase(std::remove(perms.begin(), perms.end(), permission), perms.end());

    return result<void>::ok();
}

result<void> local_auth_backend::unlock_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-1, "User not found", "local_auth"});
    }

    it->second.failed_attempts = 0;
    it->second.locked_until = std::chrono::system_clock::time_point{};

    return result<void>::ok();
}

size_t local_auth_backend::user_count() const {
    std::lock_guard<std::mutex> lock(users_mutex_);
    return users_.size();
}

size_t local_auth_backend::session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

std::string local_auth_backend::hash_password(const std::string& password) const {
    // Add a salt prefix for basic security
    std::string salted = "db_salt_" + password + "_" + config_.name;
    return sha256_hash(salted);
}

bool local_auth_backend::verify_password(const std::string& password, const std::string& hash) const {
    return hash_password(password) == hash;
}

result<void> local_auth_backend::validate_password_policy(const std::string& password) const {
    if (password.length() < config_.min_password_length) {
        return result<void>(error_info{
            -10,
            "Password must be at least " + std::to_string(config_.min_password_length) + " characters",
            "local_auth"
        });
    }

    if (config_.require_uppercase) {
        bool has_upper = std::any_of(password.begin(), password.end(),
            [](unsigned char c) { return std::isupper(c); });
        if (!has_upper) {
            return result<void>(error_info{-11, "Password must contain uppercase letter", "local_auth"});
        }
    }

    if (config_.require_lowercase) {
        bool has_lower = std::any_of(password.begin(), password.end(),
            [](unsigned char c) { return std::islower(c); });
        if (!has_lower) {
            return result<void>(error_info{-12, "Password must contain lowercase letter", "local_auth"});
        }
    }

    if (config_.require_digit) {
        bool has_digit = std::any_of(password.begin(), password.end(),
            [](unsigned char c) { return std::isdigit(c); });
        if (!has_digit) {
            return result<void>(error_info{-13, "Password must contain digit", "local_auth"});
        }
    }

    if (config_.require_special) {
        bool has_special = std::any_of(password.begin(), password.end(),
            [](unsigned char c) { return std::ispunct(c); });
        if (!has_special) {
            return result<void>(error_info{-14, "Password must contain special character", "local_auth"});
        }
    }

    return result<void>::ok();
}

std::string local_auth_backend::generate_token() const {
    return "tok_" + generate_random_string(32);
}

bool local_auth_backend::is_locked(const local_user& user) const {
    if (user.failed_attempts < config_.max_failed_attempts) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    return now < user.locked_until;
}

void local_auth_backend::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto now = std::chrono::system_clock::now();

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now > it->second.expires_at) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace database::gateway::auth
