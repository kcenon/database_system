# Database Gateway API Reference

## Overview

The Database Gateway provides a centralized access point for database operations
with built-in routing, caching, and load balancing capabilities.

## Quick Start

```cpp
#include <database/gateway/database_gateway.h>

using namespace database::gateway;

// Create gateway
database_gateway gateway;

// Configure and start
auto start_result = gateway.start(8080, security_config{});
if (start_result.is_err()) {
    std::cerr << "Failed to start gateway" << std::endl;
    return 1;
}

// Execute queries
auto result = gateway.execute_query("SELECT * FROM users WHERE active = true");
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        // Process row
    }
}

// Stop gateway
gateway.stop();
```

## Configuration

### gateway_config (via configure_routing)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| strategy | routing_strategy | ROUND_ROBIN | Query routing strategy |
| enable_read_write_split | bool | true | Route reads to replicas |

### routing_strategy

- `ROUND_ROBIN`: Distribute queries evenly across backends
- `LEAST_CONNECTIONS`: Route to backend with fewest active connections
- `RANDOM`: Random backend selection
- `WEIGHTED`: Weight-based distribution
- `LATENCY_BASED`: Route to lowest latency backend

### cache_config (via configure_caching)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| enabled | bool | true | Enable query caching |
| max_entries | size_t | 10000 | Maximum cache entries |
| default_ttl | seconds | 300 | Cache TTL in seconds |
| eviction_policy | eviction_policy | LRU | Cache eviction policy |

### audit_config (via configure_audit_logging)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| enabled | bool | true | Enable audit logging |
| log_all_queries | bool | false | Log all queries |
| log_slow_queries_ms | uint32_t | 1000 | Slow query threshold (ms) |
| log_failed_queries | bool | true | Log failed queries |
| audit_log_path | string | "" | Path to audit log file |

## API Reference

### Lifecycle Methods

#### start()
```cpp
result<void> start(uint16_t port, const security_config& security);
```
Starts the gateway server on the specified port.

**Parameters**:
- `port`: Port number to listen on
- `security`: Security configuration (TLS, authentication)

**Returns**: `result<void>` indicating success or failure.

**Example**:
```cpp
security_config security;
security.require_tls = true;
security.cert_path = "/path/to/cert.pem";

auto result = gateway.start(8080, security);
if (result.is_err()) {
    std::cerr << "Failed: " << result.error().message << std::endl;
}
```

#### stop()
```cpp
void stop();
```
Gracefully shuts down the gateway, closing all connections.

#### is_running()
```cpp
bool is_running() const;
```
Returns whether the gateway is currently running.

### Cluster Management

#### set_cluster()
```cpp
void set_cluster(std::shared_ptr<distributed::cluster_manager> cluster);
```
Assigns a cluster manager for distributed query routing.

**Example**:
```cpp
auto cluster = std::make_shared<distributed::cluster_manager>();
// ... configure cluster nodes ...
gateway.set_cluster(cluster);
```

### Query Execution

#### execute_query()
```cpp
result<core::database_result> execute_query(const std::string& query);
```
Executes a query with automatic routing based on query type.

**Parameters**:
- `query`: SQL query string

**Returns**: `result<database_result>` containing rows or error.

**Example**:
```cpp
auto result = gateway.execute_query("SELECT id, name FROM users");
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        auto id = std::get<int64_t>(row.at("id"));
        auto name = std::get<std::string>(row.at("name"));
        std::cout << id << ": " << name << std::endl;
    }
}
```

### Configuration Methods

#### configure_routing()
```cpp
void configure_routing(const routing_config& config);
```
Configures query routing behavior.

**Example**:
```cpp
routing_config routing;
routing.strategy = routing_strategy::LEAST_CONNECTIONS;
routing.read_write_split = true;
gateway.configure_routing(routing);
```

#### configure_caching()
```cpp
void configure_caching(const cache_config& config);
```
Configures query result caching.

**Example**:
```cpp
cache_config cache;
cache.enabled = true;
cache.max_entries = 5000;
cache.default_ttl = std::chrono::seconds(120);
gateway.configure_caching(cache);
```

#### configure_audit_logging()
```cpp
void configure_audit_logging(const audit_config& config);
```
Configures audit logging behavior.

**Example**:
```cpp
audit_config audit;
audit.enabled = true;
audit.log_slow_queries_ms = 500;  // Log queries > 500ms
audit.audit_log_path = "/var/log/gateway_audit.log";
gateway.configure_audit_logging(audit);
```

### Statistics and Monitoring

#### get_stats()
```cpp
gateway_stats get_stats() const;
```
Returns gateway statistics.

**Returns**: `gateway_stats` structure containing:
- `total_queries`: Total queries processed
- `successful_queries`: Number of successful queries
- `failed_queries`: Number of failed queries
- `cache_hits`: Number of cache hits
- `cache_misses`: Number of cache misses
- `avg_latency_ms`: Average query latency

**Example**:
```cpp
auto stats = gateway.get_stats();
double hit_rate = (double)stats.cache_hits /
                  (stats.cache_hits + stats.cache_misses) * 100;
std::cout << "Cache hit rate: " << hit_rate << "%" << std::endl;
```

### Authentication

#### authenticate()
```cpp
result<std::string> authenticate(const std::string& username,
                                  const std::string& password);
```
Authenticates a user and returns a session token.

**Parameters**:
- `username`: User name
- `password`: User password

**Returns**: `result<string>` containing session token or error.

**Example**:
```cpp
auto auth_result = gateway.authenticate("admin", "password123");
if (auth_result.is_ok()) {
    std::string token = auth_result.value();
    // Use token for subsequent requests
}
```

#### add_user()
```cpp
bool add_user(const std::string& username, const std::string& password_hash);
```
Adds a new user for authentication.

## Error Handling

```cpp
auto result = gateway.execute_query("SELECT * FROM users");
if (result.is_err()) {
    const auto& error = result.error();
    std::cerr << "Query failed: " << error.message << std::endl;
    std::cerr << "Error code: " << error.code << std::endl;
    std::cerr << "Context: " << error.context << std::endl;
}
```

## Thread Safety

The Database Gateway is thread-safe. Multiple threads can execute queries
concurrently without external synchronization.

## Best Practices

1. **Use read/write splitting** for read-heavy workloads
2. **Configure appropriate cache TTL** based on data freshness requirements
3. **Monitor cache hit rates** to optimize caching strategy
4. **Set connection limits** appropriate for your backend capacity
5. **Enable audit logging** for security and debugging purposes

## See Also

- [Replication API Reference](REPLICATION_API.md)
- [Distributed Setup Guide](../guides/DISTRIBUTED_SETUP.md)
- [Cluster Manager Documentation](CLUSTER_MANAGER_API.md)
