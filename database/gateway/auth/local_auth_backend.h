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
 * @brief User record for local authentication
 */
struct local_user {
    std::string user_id;
    std::string username;
    std::string password_hash;
    std::vector<std::string> groups;
    std::vector<std::string> permissions;
    bool enabled{true};
    size_t failed_attempts{0};
    std::chrono::system_clock::time_point locked_until{};
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
};

/**
 * @brief Session record for local authentication
 */
struct local_session {
    std::string token;
    std::string user_id;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
};

/**
 * @brief Local authentication backend implementation
 *
 * Provides local user database authentication with:
 * - Password hashing (Argon2, bcrypt, SHA-256)
 * - Account lockout on failed attempts
 * - Session management
 * - Permission-based authorization
 */
class local_auth_backend : public auth_backend_interface {
public:
    /**
     * @brief Construct local auth backend with configuration
     * @param config Local authentication configuration
     */
    explicit local_auth_backend(const local_config& config);

    ~local_auth_backend() override;

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

    // Local-specific methods

    /**
     * @brief Add a new user
     * @param username Username
     * @param password Plain text password (will be hashed)
     * @param groups User groups
     * @param permissions User permissions
     * @return result::ok() on success, error on failure
     */
    result<void> add_user(
        const std::string& username,
        const std::string& password,
        const std::vector<std::string>& groups = {},
        const std::vector<std::string>& permissions = {});

    /**
     * @brief Remove a user
     * @param username Username
     * @return result::ok() on success, error if user not found
     */
    result<void> remove_user(const std::string& username);

    /**
     * @brief Update user password
     * @param username Username
     * @param new_password New password
     * @return result::ok() on success, error on failure
     */
    result<void> update_password(
        const std::string& username,
        const std::string& new_password);

    /**
     * @brief Grant permission to user
     * @param username Username
     * @param permission Permission to grant
     * @return result::ok() on success
     */
    result<void> grant_permission(
        const std::string& username,
        const std::string& permission);

    /**
     * @brief Revoke permission from user
     * @param username Username
     * @param permission Permission to revoke
     * @return result::ok() on success
     */
    result<void> revoke_permission(
        const std::string& username,
        const std::string& permission);

    /**
     * @brief Unlock a locked user account
     * @param username Username
     * @return result::ok() on success
     */
    result<void> unlock_user(const std::string& username);

    /**
     * @brief Get user count
     * @return Number of registered users
     */
    [[nodiscard]] size_t user_count() const;

    /**
     * @brief Get active session count
     * @return Number of active sessions
     */
    [[nodiscard]] size_t session_count() const;

private:
    /**
     * @brief Hash password using configured algorithm
     */
    std::string hash_password(const std::string& password) const;

    /**
     * @brief Verify password against hash
     */
    bool verify_password(const std::string& password, const std::string& hash) const;

    /**
     * @brief Validate password against policy
     */
    result<void> validate_password_policy(const std::string& password) const;

    /**
     * @brief Generate a secure session token
     */
    std::string generate_token() const;

    /**
     * @brief Check if user account is locked
     */
    bool is_locked(const local_user& user) const;

    /**
     * @brief Clean up expired sessions
     */
    void cleanup_expired_sessions();

    local_config config_;
    std::atomic<bool> initialized_{false};

    mutable std::mutex users_mutex_;
    std::unordered_map<std::string, local_user> users_;  // username -> user

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, local_session> sessions_;  // token -> session
};

} // namespace database::gateway::auth
