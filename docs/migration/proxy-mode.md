# ProxyMode Migration Guide

## Overview

This guide helps you migrate from local connection pooling (`connection_pool`, `connection_pool_v2`, `connection_pool_v3`) to **ProxyMode** with `database_server` middleware.

### Why Migrate?

ProxyMode offers significant advantages over local connection pooling:

| Feature | Local Pooling | ProxyMode |
|---------|--------------|-----------|
| Connection Management | Per-application | Centralized |
| Resource Efficiency | Each app maintains own pool | Shared pool across apps |
| Load Balancing | Not available | Built-in |
| Health Monitoring | Per-application | Centralized |
| Credential Management | Local config files | Secure server-side |
| Scalability | Limited by app resources | Scales independently |

## Migration Steps

### Step 1: Update Dependencies

Ensure your project uses the latest `database_system` version with ProxyMode support:

```cmake
# CMakeLists.txt - No changes required
# ProxyMode is included in the standard database_system library
```

### Step 2: Update Connection Configuration

**Before (DirectMode with local pooling):**
```cpp
#include "database/database_manager.h"
#include "database/connection_pool.h"

auto context = std::make_shared<database::database_context>();
auto db_mgr = std::make_shared<database::database_manager>(context);

// Set database mode
db_mgr->set_mode(database::database_types::postgres);

// Create local connection pool
database::connection_pool_config pool_config;
pool_config.min_connections = 5;
pool_config.max_connections = 20;
pool_config.connection_string = "host=localhost port=5432 dbname=mydb user=user password=pass";
db_mgr->create_connection_pool(database::database_types::postgres, pool_config);

// Connect using connection string
db_mgr->connect(pool_config.connection_string);
```

**After (ProxyMode):**
```cpp
#include "database/database_manager.h"
#include "database/proxy/proxy_config.h"

auto context = std::make_shared<database::database_context>();
auto db_mgr = std::make_shared<database::database_manager>(context);

// Configure proxy connection
database::proxy::proxy_connection_config proxy_config;
proxy_config.server_host = "db-gateway.internal";  // database_server host
proxy_config.server_port = 9432;                    // database_server port
proxy_config.auth_token = "your-auth-token";        // Obtain from admin
proxy_config.connection_timeout = std::chrono::milliseconds{5000};
proxy_config.query_timeout = std::chrono::milliseconds{30000};

// Set proxy mode (pooling handled server-side)
db_mgr->set_mode_proxy(database::database_types::postgres, proxy_config);

// Connect (connection string ignored in proxy mode)
db_mgr->connect("");
```

### Step 3: Remove Local Pooling Code

Remove any direct usage of deprecated classes:

```cpp
// Remove these includes and usages:
// #include "database/connection_pool.h"           // Deprecated
// #include "database/pooling/connection_pool_v2.h" // Deprecated
// #include "database/pooling/connection_pool_v3.h" // Deprecated
// #include "database/resilience/connection_health_monitor.h" // Deprecated
// #include "database/resilience/resilient_database_connection.h" // Deprecated

// Remove pool creation:
// db_mgr->create_connection_pool(...)  // No longer needed
// db_mgr->get_connection_pool(...)      // No longer needed
```

### Step 4: Update Query Execution

Query execution remains the same:

```cpp
// These work identically in both modes
auto result = db_mgr->select_query("SELECT * FROM users");
db_mgr->insert_query("INSERT INTO users (name) VALUES ('John')");
db_mgr->update_query("UPDATE users SET name = 'Jane' WHERE id = 1");
db_mgr->delete_query("DELETE FROM users WHERE id = 1");
```

### Step 5: Handle Connection Mode Checks

You can check the current connection mode:

```cpp
if (db_mgr->current_connection_mode() == database::connection_mode::proxy) {
    // Running in proxy mode
    // Pool statistics are handled server-side
} else {
    // Running in direct mode (legacy)
    auto stats = db_mgr->get_pool_stats();
}
```

## Configuration Options

### Proxy Connection Config

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `server_host` | string | "localhost" | database_server hostname |
| `server_port` | uint16_t | 9432 | database_server port |
| `auth_token` | string | "" | Authentication token |
| `connection_timeout` | milliseconds | 5000 | Connection timeout |
| `query_timeout` | milliseconds | 30000 | Query execution timeout |
| `retry_count` | uint8_t | 3 | Connection retry attempts |
| `retry_delay` | milliseconds | 1000 | Delay between retries |
| `use_tls` | bool | true | Enable TLS encryption |
| `ca_cert_path` | string | "" | Custom CA certificate path |
| `client_cert_path` | string | "" | Client certificate (mTLS) |
| `client_key_path` | string | "" | Client private key (mTLS) |

### Example: TLS Configuration

```cpp
database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.example.com";
config.server_port = 9432;
config.auth_token = "secure-token";
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/db-gateway-ca.pem";

// For mutual TLS (mTLS):
config.client_cert_path = "/etc/ssl/certs/client.pem";
config.client_key_path = "/etc/ssl/private/client-key.pem";
```

## Fallback Strategy

During migration, you can support both modes:

```cpp
bool use_proxy = config.get_bool("database.use_proxy", true);

if (use_proxy) {
    // Try proxy mode first
    database::proxy::proxy_connection_config proxy_config;
    proxy_config.server_host = config.get_string("database.proxy_host");
    proxy_config.server_port = config.get_int("database.proxy_port");
    proxy_config.auth_token = config.get_string("database.proxy_token");

    if (db_mgr->set_mode_proxy(database::database_types::postgres, proxy_config)) {
        if (db_mgr->connect("")) {
            // Proxy mode connected successfully
            return;
        }
    }
    // Fall back to direct mode if proxy fails
}

// Direct mode fallback
db_mgr->set_mode(database::database_types::postgres);
db_mgr->connect(config.get_string("database.connection_string"));
```

## Deprecated Classes Reference

The following classes are deprecated and will be removed in a future release:

| Class | Replacement |
|-------|-------------|
| `connection_pool` | ProxyMode (server-side pooling) |
| `connection_pool_v2` | ProxyMode (server-side pooling) |
| `connection_pool_v3` | ProxyMode (server-side pooling) |
| `connection_pool_manager` | ProxyMode (server-side management) |
| `connection_health_monitor` | ProxyMode (server-side monitoring) |
| `resilient_database_connection` | ProxyMode (server-side resilience) |

## Troubleshooting

### Connection Refused

```
Error: database_server not available at db-gateway:9432
```

**Solution:** Verify that `database_server` is running and accessible:
```bash
telnet db-gateway.internal 9432
```

### Authentication Failed

```
Error: Invalid auth token
```

**Solution:** Verify your auth token is correct and not expired.

### TLS Handshake Failed

```
Error: TLS handshake failed
```

**Solution:** Check certificate paths and ensure the CA certificate is trusted.

## Related Documentation

- [database_server Setup Guide](https://github.com/kcenon/database_server)
- [Architecture Overview](../ARCHITECTURE.md)
- [API Reference](../API_REFERENCE.md)

## Migration Timeline

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 4.1 | Complete | ProxyMode infrastructure added |
| Phase 4.2 | Complete | Deprecation warnings added |
| Phase 4.3 | Pending | Pooling code removal |

---

*Last updated: Phase 4.2*
