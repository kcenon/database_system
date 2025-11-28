/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include "auth_backend.h"
#include "local_auth_backend.h"
#include "ldap_auth_backend.h"
#include "oauth_auth_backend.h"

#include <memory>
#include <variant>

namespace database::gateway::auth {

/**
 * @brief Configuration variant for all authentication backends
 */
using auth_config_variant = std::variant<local_config, ldap_config, oauth_config>;

/**
 * @brief Factory for creating authentication backends
 *
 * Provides a centralized way to create and configure authentication backends.
 * Supports local, LDAP, and OAuth authentication methods.
 *
 * Example usage:
 * @code
 *   // Create local auth backend
 *   local_config local_cfg;
 *   local_cfg.name = "local";
 *   auto local_backend = auth_backend_factory::create(local_cfg);
 *
 *   // Create LDAP auth backend
 *   ldap_config ldap_cfg;
 *   ldap_cfg.server_url = "ldap://ldap.example.com:389";
 *   ldap_cfg.base_dn = "dc=example,dc=com";
 *   auto ldap_backend = auth_backend_factory::create(ldap_cfg);
 *
 *   // Create OAuth auth backend
 *   oauth_config oauth_cfg;
 *   oauth_cfg.client_id = "my-client-id";
 *   oauth_cfg.issuer = "https://auth.example.com";
 *   auto oauth_backend = auth_backend_factory::create(oauth_cfg);
 *
 *   // Create from string type
 *   auto backend = auth_backend_factory::create_from_type("ldap", ldap_cfg);
 * @endcode
 */
class auth_backend_factory {
public:
    /**
     * @brief Create authentication backend from local configuration
     * @param config Local authentication configuration
     * @return Unique pointer to local auth backend
     */
    static std::unique_ptr<auth_backend_interface> create(const local_config& config) {
        return std::make_unique<local_auth_backend>(config);
    }

    /**
     * @brief Create authentication backend from LDAP configuration
     * @param config LDAP authentication configuration
     * @return Unique pointer to LDAP auth backend
     */
    static std::unique_ptr<auth_backend_interface> create(const ldap_config& config) {
        return std::make_unique<ldap_auth_backend>(config);
    }

    /**
     * @brief Create authentication backend from OAuth configuration
     * @param config OAuth authentication configuration
     * @return Unique pointer to OAuth auth backend
     */
    static std::unique_ptr<auth_backend_interface> create(const oauth_config& config) {
        return std::make_unique<oauth_auth_backend>(config);
    }

    /**
     * @brief Create authentication backend from configuration variant
     * @param config Configuration variant
     * @return Unique pointer to auth backend
     */
    static std::unique_ptr<auth_backend_interface> create(const auth_config_variant& config) {
        return std::visit([](auto&& cfg) -> std::unique_ptr<auth_backend_interface> {
            return create(cfg);
        }, config);
    }

    /**
     * @brief Create authentication backend from type string
     * @param type Backend type ("local", "ldap", "oauth")
     * @param config Configuration variant matching the type
     * @return Unique pointer to auth backend, nullptr if type doesn't match config
     */
    static std::unique_ptr<auth_backend_interface> create_from_type(
        const std::string& type,
        const auth_config_variant& config)
    {
        if (type == "local") {
            if (auto* cfg = std::get_if<local_config>(&config)) {
                return create(*cfg);
            }
        } else if (type == "ldap") {
            if (auto* cfg = std::get_if<ldap_config>(&config)) {
                return create(*cfg);
            }
        } else if (type == "oauth") {
            if (auto* cfg = std::get_if<oauth_config>(&config)) {
                return create(*cfg);
            }
        }
        return nullptr;
    }

    /**
     * @brief Create default local authentication backend
     * @return Unique pointer to local auth backend with default settings
     */
    static std::unique_ptr<auth_backend_interface> create_default_local() {
        local_config config;
        config.name = "default_local";
        return create(config);
    }

    /**
     * @brief Get backend type from configuration
     * @param config Configuration variant
     * @return Backend type enum
     */
    static auth_backend_type get_type(const auth_config_variant& config) {
        return std::visit([](auto&& cfg) -> auth_backend_type {
            using T = std::decay_t<decltype(cfg)>;
            if constexpr (std::is_same_v<T, local_config>) {
                return auth_backend_type::local;
            } else if constexpr (std::is_same_v<T, ldap_config>) {
                return auth_backend_type::ldap;
            } else if constexpr (std::is_same_v<T, oauth_config>) {
                return auth_backend_type::oauth;
            }
        }, config);
    }

    /**
     * @brief Get backend type string
     * @param type Backend type enum
     * @return Type as string
     */
    static std::string type_to_string(auth_backend_type type) {
        switch (type) {
            case auth_backend_type::local: return "local";
            case auth_backend_type::ldap: return "ldap";
            case auth_backend_type::oauth: return "oauth";
            default: return "unknown";
        }
    }

    /**
     * @brief Parse backend type from string
     * @param type_str Type string
     * @return Backend type enum, local if unknown
     */
    static auth_backend_type string_to_type(const std::string& type_str) {
        if (type_str == "ldap") return auth_backend_type::ldap;
        if (type_str == "oauth") return auth_backend_type::oauth;
        return auth_backend_type::local;
    }
};

/**
 * @brief Authentication manager for multiple backends
 *
 * Manages multiple authentication backends and provides unified access.
 * Supports fallback authentication across multiple backends.
 */
class auth_manager {
public:
    /**
     * @brief Add an authentication backend
     * @param backend Authentication backend
     * @param primary Set as primary backend
     */
    void add_backend(std::unique_ptr<auth_backend_interface> backend, bool primary = false) {
        if (primary || backends_.empty()) {
            backends_.insert(backends_.begin(), std::move(backend));
        } else {
            backends_.push_back(std::move(backend));
        }
    }

    /**
     * @brief Initialize all backends
     * @return result::ok() if at least one backend initializes successfully
     */
    result<void> initialize() {
        if (backends_.empty()) {
            return result<void>(error_info{-1, "No authentication backends configured", "auth_manager"});
        }

        bool any_initialized = false;
        for (auto& backend : backends_) {
            auto init_result = backend->initialize();
            if (init_result.is_ok()) {
                any_initialized = true;
            }
        }

        if (!any_initialized) {
            return result<void>(error_info{-2, "Failed to initialize any authentication backend", "auth_manager"});
        }

        return result<void>::ok();
    }

    /**
     * @brief Shutdown all backends
     */
    void shutdown() {
        for (auto& backend : backends_) {
            backend->shutdown();
        }
    }

    /**
     * @brief Authenticate using backends in order
     * @param credentials User credentials
     * @return Authentication result from first successful backend
     */
    result<auth_result> authenticate(const auth_credentials& credentials) {
        for (auto& backend : backends_) {
            if (!backend->is_healthy()) continue;

            auto auth_result = backend->authenticate(credentials);
            if (auth_result.is_ok()) {
                return auth_result;
            }
        }

        return result<auth_result>(error_info{-3, "Authentication failed on all backends", "auth_manager"});
    }

    /**
     * @brief Validate token on any backend
     * @param token Access token
     * @return Authentication result if valid
     */
    result<auth_result> validate_token(const std::string& token) {
        for (auto& backend : backends_) {
            if (!backend->is_healthy()) continue;

            auto validate_result = backend->validate_token(token);
            if (validate_result.is_ok()) {
                return validate_result;
            }
        }

        return result<auth_result>(error_info{-4, "Token validation failed on all backends", "auth_manager"});
    }

    /**
     * @brief Check if user has permission on any backend
     * @param user_id User identifier
     * @param permission Required permission
     * @return true if any backend grants the permission
     */
    bool has_permission(const std::string& user_id, const std::string& permission) const {
        for (const auto& backend : backends_) {
            if (backend->has_permission(user_id, permission)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get all permissions across all backends
     * @param user_id User identifier
     * @return Combined permissions from all backends
     */
    std::vector<std::string> get_permissions(const std::string& user_id) const {
        std::vector<std::string> all_permissions;

        for (const auto& backend : backends_) {
            auto perms = backend->get_permissions(user_id);
            for (const auto& perm : perms) {
                if (std::find(all_permissions.begin(), all_permissions.end(), perm) == all_permissions.end()) {
                    all_permissions.push_back(perm);
                }
            }
        }

        return all_permissions;
    }

    /**
     * @brief Check if any backend is healthy
     * @return true if at least one backend is healthy
     */
    bool is_healthy() const {
        for (const auto& backend : backends_) {
            if (backend->is_healthy()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get backend count
     * @return Number of configured backends
     */
    size_t backend_count() const {
        return backends_.size();
    }

    /**
     * @brief Get backend by index
     * @param index Backend index
     * @return Pointer to backend or nullptr
     */
    auth_backend_interface* get_backend(size_t index) {
        if (index >= backends_.size()) return nullptr;
        return backends_[index].get();
    }

    /**
     * @brief Get backend by name
     * @param name Backend name
     * @return Pointer to backend or nullptr
     */
    auth_backend_interface* get_backend_by_name(const std::string& name) {
        for (auto& backend : backends_) {
            if (backend->name() == name) {
                return backend.get();
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<auth_backend_interface>> backends_;
};

} // namespace database::gateway::auth
