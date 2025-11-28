/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "database_gateway.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <sstream>

namespace database::gateway {

namespace {

/**
 * @brief Hash a query string for cache key
 */
std::string hash_query(const std::string& query) {
    // Simple hash using std::hash
    std::hash<std::string> hasher;
    return std::to_string(hasher(query));
}

/**
 * @brief Check if query is a SELECT statement (cacheable)
 */
bool is_select_query(const std::string& query) {
    // Trim and convert to uppercase for checking
    std::string trimmed = query;
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return false;

    std::string upper;
    upper.reserve(7);
    for (size_t i = start; i < std::min(start + 6, trimmed.size()); ++i) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(trimmed[i])));
    }

    return upper == "SELECT";
}

} // anonymous namespace

// Constructor
database_gateway::database_gateway() = default;

// Destructor
database_gateway::~database_gateway() {
    stop();
}

result<void> database_gateway::start(uint16_t port, const security_config& security) {
    if (running_.load()) {
        return result<void>(error_info{-1, "Gateway already running", "gateway"});
    }

    port_ = port;
    security_ = security;

    // Note: Audit logging disabled - requires proper CMake setup

    // Note: Actual server start would require network_system integration
    // For now, we mark as running and rely on execute_query being called directly
    running_.store(true);

    return result<void>::ok();
}

void database_gateway::stop() {
    if (!running_.load()) return;

    running_.store(false);

    if (server_) {
        server_->stop();
        server_.reset();
    }

    // Stop audit logger
    if (audit_logger_) {
        audit_logger_->stop();
        audit_logger_.reset();
    }
}

result<void> database_gateway::register_cluster(
    const std::string& cluster_id,
    std::shared_ptr<distributed::cluster_manager> cluster
) {
    if (cluster_id.empty()) {
        return result<void>(error_info{-2, "Cluster ID cannot be empty", "gateway"});
    }

    if (!cluster) {
        return result<void>(error_info{-2, "Cluster cannot be null", "gateway"});
    }

    std::lock_guard<std::mutex> lock(clusters_mutex_);

    if (clusters_.find(cluster_id) != clusters_.end()) {
        return result<void>(error_info{-3, "Cluster already registered: " + cluster_id, "gateway"});
    }

    clusters_[cluster_id] = cluster;
    return result<void>::ok();
}

result<void> database_gateway::add_routing_rule(const routing_rule& rule) {
    if (rule.name.empty()) {
        return result<void>(error_info{-2, "Rule name cannot be empty", "gateway"});
    }

    std::lock_guard<std::mutex> lock(routing_mutex_);

    // Check for duplicate name
    for (const auto& existing : routing_rules_) {
        if (existing.name == rule.name) {
            return result<void>(error_info{-3, "Rule already exists: " + rule.name, "gateway"});
        }
    }

    routing_rules_.push_back(rule);

    // Sort by priority (higher first)
    std::sort(routing_rules_.begin(), routing_rules_.end(),
              [](const routing_rule& a, const routing_rule& b) {
                  return a.priority > b.priority;
              });

    return result<void>::ok();
}

result<void> database_gateway::remove_routing_rule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    auto it = std::find_if(routing_rules_.begin(), routing_rules_.end(),
                           [&rule_name](const routing_rule& r) {
                               return r.name == rule_name;
                           });

    if (it == routing_rules_.end()) {
        return result<void>(error_info{-4, "Rule not found: " + rule_name, "gateway"});
    }

    routing_rules_.erase(it);
    return result<void>::ok();
}

void database_gateway::configure_cache(const cache_config& config) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_config_ = config;

    if (!config.enabled) {
        query_cache_.clear();
    }
}

void database_gateway::configure_audit_logging(const audit_config& config) {
    audit_config_ = config;

    // Stop existing audit logger if any
    if (audit_logger_) {
        audit_logger_->stop();
        audit_logger_.reset();
    }

    // Create and start new audit logger if enabled
    if (config.enabled && !config.audit_log_path.empty()) {
        audit_logger_config logger_config;
        logger_config.log_path = config.audit_log_path;
        logger_config.format = config.format;
        logger_config.max_file_size_mb = config.max_file_size_mb;
        logger_config.max_files = config.max_files;
        logger_config.async_write = config.async_write;

        audit_logger_ = std::make_unique<audit_logger>(logger_config);
        audit_logger_->start();
    }
}

result<core::database_result> database_gateway::execute_query(const std::string& query) {
    auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    core::database_result final_result;
    std::string target_cluster_id;
    std::string error_msg;

    // Check cache first (for SELECT queries only)
    if (cache_config_.enabled && is_cacheable(query)) {
        auto cached = get_cached_result(query);
        if (cached.has_value()) {
            cache_hits_.fetch_add(1);
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time
            );
            audit_log_query(query, true, duration, "cache", "");
            return result<core::database_result>::ok(cached.value());
        }
        cache_misses_.fetch_add(1);
    }

    // Route query to appropriate cluster
    std::string cluster_id = route_query(query);
    target_cluster_id = cluster_id;

    std::shared_ptr<distributed::cluster_manager> target_cluster;
    {
        std::lock_guard<std::mutex> lock(clusters_mutex_);

        if (cluster_id.empty()) {
            // Use first cluster if no routing rule matches
            if (clusters_.empty()) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time
                );
                audit_log_query(query, false, duration, "", "No clusters registered");
                return result<core::database_result>(
                    error_info{-5, "No clusters registered", "gateway"}
                );
            }
            target_cluster = clusters_.begin()->second;
            target_cluster_id = clusters_.begin()->first;
        } else {
            auto it = clusters_.find(cluster_id);
            if (it == clusters_.end()) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time
                );
                audit_log_query(query, false, duration, cluster_id, "Cluster not found");
                return result<core::database_result>(
                    error_info{-5, "Cluster not found: " + cluster_id, "gateway"}
                );
            }
            target_cluster = it->second;
        }
    }

    // Execute query on cluster
    result<core::database_result> query_result(error_info{-1, "Unknown error", "gateway"});

    if (is_select_query(query)) {
        query_result = target_cluster->execute_read_query(query);
    } else {
        auto write_result = target_cluster->execute_write_query(query);
        if (write_result.is_ok()) {
            // Convert write result to database_result (empty result for write operations)
            core::database_result result_data;
            // Note: database_result is std::vector<database_row>, so we return empty
            // The rows_affected count is available in write_result.value() but not stored
            (void)write_result.value();  // Suppress unused warning
            query_result = result<core::database_result>::ok(result_data);
        } else {
            query_result = result<core::database_result>(write_result.error());
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );

    success = query_result.is_ok();

    if (!success) {
        error_msg = query_result.error().message;
    }

    if (success && cache_config_.enabled && is_cacheable(query)) {
        cache_result(query, query_result.value());
    }

    audit_log_query(query, success, duration, target_cluster_id, error_msg);

    return query_result;
}

result<void> database_gateway::authenticate(
    const std::string& username,
    const std::string& password
) {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return result<void>(error_info{-6, "Authentication failed: user not found", "gateway"});
    }

    // Simple password comparison (in production, use proper hashing)
    if (it->second != password) {
        return result<void>(error_info{-6, "Authentication failed: invalid password", "gateway"});
    }

    return result<void>::ok();
}

bool database_gateway::is_authorized(
    const std::string& username,
    const std::string& permission
) const {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    auto it = permissions_.find(username);
    if (it == permissions_.end()) {
        return false;
    }

    const auto& user_perms = it->second;
    return std::find(user_perms.begin(), user_perms.end(), permission) != user_perms.end();
}

double database_gateway::get_cache_hit_rate() const {
    uint64_t hits = cache_hits_.load();
    uint64_t misses = cache_misses_.load();
    uint64_t total = hits + misses;

    if (total == 0) return 0.0;
    return static_cast<double>(hits) / static_cast<double>(total);
}

std::map<std::string, uint64_t> database_gateway::get_cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    return {
        {"hits", cache_hits_.load()},
        {"misses", cache_misses_.load()},
        {"entries", query_cache_.size()},
        {"max_size", cache_config_.max_size}
    };
}

void database_gateway::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    query_cache_.clear();
    cache_hits_.store(0);
    cache_misses_.store(0);
}

std::string database_gateway::route_query(const std::string& query) {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    for (const auto& rule : routing_rules_) {
        try {
            if (std::regex_search(query, rule.pattern)) {
                return rule.target_cluster;
            }
        } catch (const std::regex_error&) {
            // Skip invalid regex patterns
            continue;
        }
    }

    return ""; // No matching rule
}

bool database_gateway::is_cacheable(const std::string& query) const {
    return is_select_query(query);
}

std::optional<core::database_result> database_gateway::get_cached_result(
    const std::string& query
) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    std::string key = hash_query(query);
    auto it = query_cache_.find(key);

    if (it == query_cache_.end()) {
        return std::nullopt;
    }

    // Check TTL
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        now - it->second.timestamp
    );

    if (age > cache_config_.ttl) {
        query_cache_.erase(it);
        return std::nullopt;
    }

    it->second.hit_count++;
    return it->second.result;
}

void database_gateway::cache_result(
    const std::string& query,
    const core::database_result& result
) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Evict if at capacity
    if (query_cache_.size() >= cache_config_.max_size) {
        evict_cache_lru();
    }

    std::string key = hash_query(query);
    cache_entry entry;
    entry.result = result;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.hit_count = 0;

    query_cache_[key] = entry;
}

void database_gateway::audit_log_query(
    const std::string& query,
    bool success,
    std::chrono::milliseconds duration,
    const std::string& target_cluster,
    const std::string& error_msg
) {
    // Check if audit logging is enabled
    if (!audit_config_.enabled || !audit_logger_) {
        return;
    }

    // Check if we should log this query
    if (!audit_config_.log_all_queries) {
        if (!success && !audit_config_.log_failed_queries) {
            return;
        }
        if (success && duration.count() < static_cast<long>(audit_config_.log_slow_queries_ms)) {
            return;
        }
    }

    // Create audit entry
    audit_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.user = "";  // TODO: Add user context when auth is integrated
    entry.session_id = "";  // TODO: Add session context
    entry.client_ip = "";  // TODO: Add client IP when network is integrated
    entry.operation = is_select_query(query) ? "SELECT" : "WRITE";
    entry.query_hash = hash_query(query);
    entry.target_cluster = target_cluster;
    entry.success = success;
    entry.latency = duration;
    entry.error_message = error_msg;

    audit_logger_->log(entry);
}

void database_gateway::evict_cache_lru() {
    // Find entry with oldest timestamp (LRU approximation)
    auto oldest_it = query_cache_.begin();
    auto oldest_time = oldest_it->second.timestamp;

    for (auto it = query_cache_.begin(); it != query_cache_.end(); ++it) {
        if (it->second.timestamp < oldest_time) {
            oldest_it = it;
            oldest_time = it->second.timestamp;
        }
    }

    if (oldest_it != query_cache_.end()) {
        query_cache_.erase(oldest_it);
    }
}

} // namespace database::gateway
