/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <regex>
#include <mutex>
#include "../core/database_backend.h"
#include "../core/result.h"
#include "../server/database_proxy_server.h"
#include "../distributed/cluster_manager.h"
#include "../protocol/database_protocol.h"
#include "audit_logger.h"
#include "auth/auth_backend_factory.h"

// network_system integration for gateway server
#include <kcenon/network/core/messaging_server.h>
#include <kcenon/network/session/messaging_session.h>

// Logging/monitoring integration
#include "../integrated/adapters/logger_adapter.h"
#include "../integrated/adapters/monitoring_adapter.h"

namespace database::gateway {

/**
 * @brief Security configuration for gateway
 */
struct security_config {
    bool enable_tls{false};                     ///< Enable TLS/SSL encryption
    std::string cert_file;                      ///< TLS certificate file path
    std::string key_file;                       ///< TLS private key file path
    bool require_auth{false};                   ///< Require authentication
    std::string auth_backend_type{"local"};     ///< Authentication backend (local, ldap, oauth)

    /// Local authentication configuration (used when auth_backend_type == "local")
    auth::local_config local_auth_config;

    /// LDAP authentication configuration (used when auth_backend_type == "ldap")
    auth::ldap_config ldap_auth_config;

    /// OAuth authentication configuration (used when auth_backend_type == "oauth")
    auth::oauth_config oauth_auth_config;
};

/**
 * @brief Query routing rule
 */
struct routing_rule {
    std::string name;                           ///< Rule name
    std::regex pattern;                         ///< Query pattern regex
    std::string target_cluster;                 ///< Target cluster ID
    int priority{0};                            ///< Rule priority (higher = higher priority)
    std::string required_permission;            ///< Required permission (if any)
};

/**
 * @brief Query cache configuration
 */
struct cache_config {
    size_t max_size{1000};                      ///< Maximum number of cached queries
    std::chrono::seconds ttl{300};              ///< Cache entry time-to-live (5 min default)
    bool enabled{false};                        ///< Enable query caching
};

/**
 * @brief Audit logging configuration
 */
struct audit_config {
    bool enabled{false};                        ///< Enable audit logging
    bool log_all_queries{false};                ///< Log all queries
    uint32_t log_slow_queries_ms{1000};         ///< Log queries slower than this (ms)
    bool log_failed_queries{true};              ///< Log failed queries
    std::string audit_log_path;                 ///< Audit log file path
    audit_format format{audit_format::JSON};    ///< Log format (JSON or CSV)
    size_t max_file_size_mb{100};               ///< Max file size before rotation
    size_t max_files{10};                       ///< Max rotated files to keep
    bool async_write{true};                     ///< Use async writing for performance
};

/**
 * @brief Gateway observability configuration
 *
 * Configures logging and monitoring integration for the gateway.
 */
struct gateway_observability_config {
    /// Enable integrated logging
    bool enable_logging{true};

    /// Enable integrated monitoring
    bool enable_monitoring{true};

    /// Logger configuration
    integrated::db_logger_config logger_config;

    /// Monitoring configuration
    integrated::db_monitoring_config monitoring_config;
};

/**
 * @brief Query cache entry
 */
struct cache_entry {
    core::database_result result;
    std::chrono::steady_clock::time_point timestamp;
    size_t hit_count{0};
};

/**
 * @class database_gateway
 * @brief Database proxy gateway for centralized database access
 *
 * Features:
 * - SQL query parsing and validation
 * - Query routing based on pattern matching
 * - LRU query caching for improved performance
 * - Authentication and authorization
 * - Audit logging for compliance (GDPR, SOC2)
 * - Rate limiting and throttling
 * - Connection pooling and management
 *
 * Architecture:
 * - Listens on TCP port via network_system
 * - Routes queries to appropriate cluster_manager instances
 * - Caches frequent queries to reduce database load
 * - Logs all operations for security auditing
 *
 * Security:
 * - TLS/SSL encryption support
 * - Authentication (local, LDAP, OAuth)
 * - Authorization based on permissions
 * - SQL injection prevention
 *
 * Example Usage:
 * @code
 *   auto gateway = std::make_shared<database_gateway>();
 *
 *   // Configure security
 *   security_config security;
 *   security.enable_tls = true;
 *   security.cert_file = "/etc/ssl/db-gateway.crt";
 *   security.key_file = "/etc/ssl/db-gateway.key";
 *   security.require_auth = true;
 *
 *   // Start gateway
 *   gateway->start(5000, security);
 *
 *   // Add routing rule
 *   routing_rule users_rule;
 *   users_rule.name = "users_routing";
 *   users_rule.pattern = std::regex("SELECT .* FROM users.*");
 *   users_rule.target_cluster = "users-cluster";
 *   users_rule.priority = 10;
 *   gateway->add_routing_rule(users_rule);
 *
 *   // Enable caching
 *   cache_config cache;
 *   cache.enabled = true;
 *   cache.max_size = 1000;
 *   cache.ttl = std::chrono::seconds(300);
 *   gateway->configure_cache(cache);
 *
 *   // Enable audit logging
 *   audit_config audit;
 *   audit.log_all_queries = true;
 *   audit.log_slow_queries_ms = 1000;
 *   gateway->configure_audit_logging(audit);
 * @endcode
 */
class database_gateway {
public:
    /**
     * @brief Construct a new database gateway
     */
    database_gateway();

    /**
     * @brief Destructor - ensures cleanup
     */
    ~database_gateway();

    /**
     * @brief Start the gateway server
     * @param port TCP port to listen on
     * @param security Security configuration
     * @return result::ok() on success, error on failure
     */
    result<void> start(uint16_t port, const security_config& security);

    /**
     * @brief Stop the gateway server
     */
    void stop();

    /**
     * @brief Check if gateway is running
     * @return true if running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Register a cluster with the gateway
     * @param cluster_id Cluster identifier
     * @param cluster Cluster manager instance
     * @return result::ok() on success, error if cluster already exists
     */
    result<void> register_cluster(const std::string& cluster_id,
                                   std::shared_ptr<distributed::cluster_manager> cluster);

    /**
     * @brief Add query routing rule
     * @param rule Routing rule
     * @return result::ok() on success, error on failure
     *
     * Rules are evaluated in priority order (highest first).
     * First matching rule determines the target cluster.
     */
    result<void> add_routing_rule(const routing_rule& rule);

    /**
     * @brief Remove routing rule
     * @param rule_name Rule name
     * @return result::ok() on success, error if rule not found
     */
    result<void> remove_routing_rule(const std::string& rule_name);

    /**
     * @brief Configure query cache
     * @param config Cache configuration
     */
    void configure_cache(const cache_config& config);

    /**
     * @brief Configure audit logging
     * @param config Audit logging configuration
     */
    void configure_audit_logging(const audit_config& config);

    /**
     * @brief Configure observability (logging and monitoring)
     * @param config Observability configuration
     * @return result::ok() on success, error on failure
     */
    result<void> configure_observability(const gateway_observability_config& config);

    /**
     * @brief Execute query through gateway
     * @param query SQL query
     * @return Query result or error
     *
     * This method:
     * 1. Checks cache (if enabled)
     * 2. Routes query to appropriate cluster
     * 3. Logs query (if audit enabled)
     * 4. Caches result (if cacheable)
     */
    result<core::database_result> execute_query(const std::string& query);

    /**
     * @brief Authenticate user credentials
     * @param username Username
     * @param password Password
     * @return result::ok() if authenticated, error otherwise
     */
    result<void> authenticate(const std::string& username, const std::string& password);

    /**
     * @brief Authenticate with credentials (advanced)
     * @param credentials Authentication credentials
     * @return Authentication result with user info and token
     */
    result<auth::auth_result> authenticate(const auth::auth_credentials& credentials);

    /**
     * @brief Validate an access token
     * @param token Access token to validate
     * @return Authentication result if valid
     */
    result<auth::auth_result> validate_token(const std::string& token);

    /**
     * @brief Refresh an expired token (for OAuth)
     * @param refresh_token Refresh token
     * @return New authentication result
     */
    result<auth::auth_result> refresh_token(const std::string& refresh_token);

    /**
     * @brief Check if user has required permission
     * @param username Username
     * @param permission Required permission
     * @return true if authorized
     */
    bool is_authorized(const std::string& username, const std::string& permission) const;

    /**
     * @brief Get authentication manager for advanced configuration
     * @return Pointer to auth manager
     */
    auth::auth_manager* get_auth_manager() { return &auth_manager_; }

    /**
     * @brief Add an authentication backend
     * @param backend Authentication backend
     * @param primary Set as primary backend
     */
    void add_auth_backend(std::unique_ptr<auth::auth_backend_interface> backend, bool primary = false);

    /**
     * @brief Get cache hit rate
     * @return Cache hit rate (0.0 - 1.0)
     */
    double get_cache_hit_rate() const;

    /**
     * @brief Get cache statistics
     * @return Map of cache statistics
     */
    std::map<std::string, uint64_t> get_cache_stats() const;

    /**
     * @brief Clear query cache
     */
    void clear_cache();

private:
    /**
     * @brief Route query to appropriate cluster
     * @param query SQL query
     * @return Cluster ID or empty string if no match
     */
    std::string route_query(const std::string& query);

    /**
     * @brief Check if query result is cacheable
     * @param query SQL query
     * @return true if cacheable (SELECT queries only)
     */
    bool is_cacheable(const std::string& query) const;

    /**
     * @brief Get cached query result
     * @param query SQL query
     * @return Cached result or empty optional
     */
    std::optional<core::database_result> get_cached_result(const std::string& query);

    /**
     * @brief Cache query result
     * @param query SQL query
     * @param result Query result
     */
    void cache_result(const std::string& query, const core::database_result& result);

    /**
     * @brief Log query for audit
     * @param query SQL query
     * @param success Query success status
     * @param duration Query execution duration
     * @param target_cluster Target cluster ID
     * @param error_msg Error message (if failed)
     */
    void audit_log_query(const std::string& query, bool success,
                         std::chrono::milliseconds duration,
                         const std::string& target_cluster = "",
                         const std::string& error_msg = "");

    /**
     * @brief Evict oldest cache entries (LRU)
     */
    void evict_cache_lru();

    // Network server handlers
    /**
     * @brief Handle client connection
     * @param session Network session
     */
    void handle_client_connect(
        std::shared_ptr<network_system::session::messaging_session> session);

    /**
     * @brief Handle client disconnection
     * @param session_id Session ID
     */
    void handle_client_disconnect(const std::string& session_id);

    /**
     * @brief Handle incoming message from network
     * @param session Network session
     * @param data Message data
     */
    void handle_message(
        std::shared_ptr<network_system::session::messaging_session> session,
        const std::vector<uint8_t>& data);

    /**
     * @brief Process query request from network
     * @param header Message header
     * @param payload Request payload
     * @return Response bytes
     */
    std::vector<uint8_t> process_network_query(
        const protocol::message_header& header,
        const std::vector<uint8_t>& payload);

    /**
     * @brief Convert database_result to protocol::query_response
     * @param db_result Database result
     * @return Protocol query response
     */
    protocol::query_response convert_to_protocol_response(
        const core::database_result& db_result);

    // Server state
    std::shared_ptr<server::database_proxy_server> server_;
    std::shared_ptr<network_system::core::messaging_server> network_server_;
    std::atomic<bool> running_{false};
    uint16_t port_{0};
    security_config security_;
    std::atomic<uint64_t> next_session_id_{1};

    // Cluster management
    mutable std::mutex clusters_mutex_;
    std::unordered_map<std::string, std::shared_ptr<distributed::cluster_manager>> clusters_;

    // Query routing
    mutable std::mutex routing_mutex_;
    std::vector<routing_rule> routing_rules_;

    // Query cache
    mutable std::mutex cache_mutex_;
    cache_config cache_config_;
    std::unordered_map<std::string, cache_entry> query_cache_;
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};

    // Audit logging
    audit_config audit_config_;
    std::unique_ptr<audit_logger> audit_logger_;

    // Authentication
    auth::auth_manager auth_manager_;
    mutable std::mutex auth_mutex_;
    std::unordered_map<std::string, std::string> users_; // username -> password hash (legacy)
    std::unordered_map<std::string, std::vector<std::string>> permissions_; // username -> permissions (legacy)

    // Observability (logging and monitoring integration)
    gateway_observability_config observability_config_;
    std::unique_ptr<integrated::adapters::logger_adapter> logger_;
    std::unique_ptr<integrated::adapters::monitoring_adapter> monitor_;
};

} // namespace database::gateway
