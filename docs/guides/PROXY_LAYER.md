# Proxy Layer Documentation

> **Status**: Development Preview (Stub Implementation)
> **Since**: Phase 4.1
> **Source**: `database/proxy/`

This document provides comprehensive documentation for the database proxy layer, which enables proxied database connections through a `database_server` middleware.

## Table of Contents

- [Architecture Overview](#architecture-overview)
  - [Purpose and Use Cases](#purpose-and-use-cases)
  - [Connector Hierarchy](#connector-hierarchy)
  - [Direct vs Proxy Mode](#direct-vs-proxy-mode)
- [Configuration (proxy\_config.h)](#configuration-proxy_configh)
  - [proxy\_connection\_config](#proxy_connection_config)
  - [proxy\_server\_info](#proxy_server_info)
  - [Configuration Examples](#configuration-examples)
- [API Reference (proxy\_connector.h)](#api-reference-proxy_connectorh)
  - [proxy\_state Enum](#proxy_state-enum)
  - [proxy\_connector Class](#proxy_connector-class)
  - [Connection Lifecycle](#connection-lifecycle)
  - [Error Handling](#error-handling)
  - [Thread Safety](#thread-safety)
- [Usage Examples](#usage-examples)
  - [Basic Proxy Setup with PostgreSQL](#basic-proxy-setup-with-postgresql)
  - [Proxy with TLS Configuration](#proxy-with-tls-configuration)
  - [Mutual TLS (mTLS) Authentication](#mutual-tls-mtls-authentication)
  - [Fallback Strategy (Proxy to Direct)](#fallback-strategy-proxy-to-direct)
- [Backend Integration](#backend-integration)
  - [Supported Database Backends](#supported-database-backends)
  - [Proxy-Specific Limitations](#proxy-specific-limitations)
- [Performance Considerations](#performance-considerations)
- [Current Status and Roadmap](#current-status-and-roadmap)
- [Related Documentation](#related-documentation)

---

## Architecture Overview

### Purpose and Use Cases

The proxy layer provides an alternative connection mode where database queries are routed through a centralized `database_server` middleware instead of connecting directly to the database engine. This architecture enables:

- **Centralized connection management**: A single server manages connection pools for multiple clients
- **Security enforcement**: Authentication and authorization at the middleware level
- **Load balancing**: Distribute queries across multiple database replicas
- **Credential isolation**: Database credentials stored server-side, not in client applications
- **Monitoring**: Centralized query logging and performance tracking

### Connector Hierarchy

The proxy connector integrates into the existing backend architecture:

```
                    database_backend (abstract)
                    ├── postgres_backend
                    ├── mysql_backend
                    ├── sqlite_backend
                    ├── mongodb_backend
                    ├── redis_backend
                    └── proxy_connector        ← Proxy layer
                        └── Sends queries to database_server
                            └── database_server routes to actual backend
```

`proxy_connector` implements the `database_backend` interface (defined in `database/core/database_backend.h`), making it interchangeable with any direct backend. The `database_manager` can switch between direct and proxy mode at runtime without changing query code.

### Direct vs Proxy Mode

| Aspect | Direct Mode | Proxy Mode |
|--------|-------------|------------|
| Connection target | Database engine directly | `database_server` middleware |
| Credential location | Client application | Server-side |
| Connection pooling | Client-managed | Server-managed |
| Network hops | 1 (client → DB) | 2 (client → server → DB) |
| Configuration class | `connection_config` | `proxy_connection_config` |
| Mode enum | `connection_mode::direct` | `connection_mode::proxy` |

```
Direct Mode:
  [Client App] ──── [PostgreSQL/MySQL/SQLite]

Proxy Mode:
  [Client App] ──── [database_server] ──── [PostgreSQL/MySQL/SQLite]
                     (middleware)
```

---

## Configuration (proxy_config.h)

**Header**: `database/proxy/proxy_config.h`
**Namespace**: `database::proxy`

### proxy_connection_config

Configuration structure for connecting to the `database_server` middleware.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `server_host` | `std::string` | `"localhost"` | Hostname or IP address of the database_server |
| `server_port` | `uint16_t` | `9432` | Port number of the database_server |
| `auth_token` | `std::string` | `""` (empty) | Authentication token for client identification |
| `connection_timeout` | `std::chrono::milliseconds` | `5000` | Timeout for establishing connection |
| `query_timeout` | `std::chrono::milliseconds` | `30000` | Timeout for query execution |
| `retry_count` | `uint8_t` | `3` | Number of retry attempts on connection failure |
| `retry_delay` | `std::chrono::milliseconds` | `1000` | Delay between retry attempts |
| `use_tls` | `bool` | `true` | Enable TLS/SSL encryption |
| `ca_cert_path` | `std::string` | `""` (empty) | Path to CA certificate (empty = system CAs) |
| `client_cert_path` | `std::string` | `""` (empty) | Path to client certificate for mTLS |
| `client_key_path` | `std::string` | `""` (empty) | Path to client private key for mTLS |

#### Validation

Call `is_valid()` to verify configuration before use:

```cpp
proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;

if (!config.is_valid()) {
    // Invalid configuration - check host, port, and timeouts
}
```

Validation checks:
- `server_host` must not be empty
- `server_port` must be greater than 0
- `connection_timeout` must be greater than 0
- `query_timeout` must be greater than 0

### proxy_server_info

Metadata about the connected `database_server`. Available after successful connection.

| Field | Type | Description |
|-------|------|-------------|
| `version` | `std::string` | Server version string |
| `server_id` | `std::string` | Unique server identifier |
| `supported_databases` | `std::string` | Comma-separated list of supported database types |
| `max_connections` | `uint32_t` | Maximum connections allowed for this client |
| `tls_enabled` | `bool` | Whether the server supports TLS |

### Configuration Examples

#### Minimal Configuration

```cpp
proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;
config.auth_token = "client-token-abc";
```

#### Production Configuration

```cpp
proxy_connection_config config;
config.server_host = "db-gateway.prod.internal";
config.server_port = 9432;
config.auth_token = get_token_from_vault();  // Use secret management
config.connection_timeout = std::chrono::milliseconds{10000};
config.query_timeout = std::chrono::milliseconds{60000};
config.retry_count = 5;
config.retry_delay = std::chrono::milliseconds{2000};
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/db-gateway-ca.pem";
```

#### Environment-Specific Patterns

```cpp
proxy_connection_config create_config_from_env() {
    proxy_connection_config config;
    config.server_host = std::getenv("DB_PROXY_HOST") ?: "localhost";
    config.server_port = static_cast<uint16_t>(
        std::atoi(std::getenv("DB_PROXY_PORT") ?: "9432"));
    config.auth_token = std::getenv("DB_PROXY_TOKEN") ?: "";
    config.use_tls = std::string(std::getenv("DB_PROXY_TLS") ?: "true") == "true";
    return config;
}
```

---

## API Reference (proxy_connector.h)

**Header**: `database/proxy/proxy_connector.h`
**Namespace**: `database::proxy`

### proxy_state Enum

Represents the connection state of the proxy connector.

| Value | Integer | Description |
|-------|---------|-------------|
| `disconnected` | 0 | Not connected to database_server |
| `connecting` | 1 | Connection in progress |
| `connected` | 2 | Connected and ready for queries |
| `error` | 3 | Connection error occurred |

Convert to string with `to_string(proxy_state)`:

```cpp
auto state = connector->state();
std::cout << "State: " << to_string(state) << std::endl;
// Output: "State: disconnected"
```

### proxy_connector Class

Implements `database::core::database_backend` for proxy mode operations.

#### Construction

```cpp
proxy_connector(database_types db_type, const proxy_connection_config& config);
```

- `db_type`: The target database type on the server side (e.g., `database_types::postgres`)
- `config`: Proxy connection configuration

The connector is **non-copyable and non-moveable** due to `std::atomic` members.

```cpp
auto connector = std::make_unique<proxy_connector>(
    database_types::postgres, config);
```

#### database_backend Interface Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `type()` | `database_types type() const` | Returns the target database type |
| `initialize()` | `VoidResult initialize(const connection_config&)` | Connects to database_server (connection_config is ignored; proxy_connection_config is used) |
| `shutdown()` | `VoidResult shutdown()` | Disconnects from database_server |
| `is_initialized()` | `bool is_initialized() const` | Returns `true` if state is `connected` |
| `insert_query()` | `Result<uint64_t> insert_query(const std::string&)` | Sends INSERT query via proxy |
| `update_query()` | `Result<uint64_t> update_query(const std::string&)` | Sends UPDATE query via proxy |
| `delete_query()` | `Result<uint64_t> delete_query(const std::string&)` | Sends DELETE query via proxy |
| `select_query()` | `Result<database_result> select_query(const std::string&)` | Sends SELECT query via proxy |
| `execute_query()` | `VoidResult execute_query(const std::string&)` | Sends general query via proxy |
| `begin_transaction()` | `VoidResult begin_transaction()` | Begins a transaction on the server |
| `commit_transaction()` | `VoidResult commit_transaction()` | Commits the current transaction |
| `rollback_transaction()` | `VoidResult rollback_transaction()` | Rolls back the current transaction |
| `in_transaction()` | `bool in_transaction() const` | Returns `true` if a transaction is active |
| `last_error()` | `std::string last_error() const` | Returns the last error message |
| `connection_info()` | `map<string,string> connection_info() const` | Returns connection metadata map |

#### Proxy-Specific Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `state()` | `proxy_state state() const noexcept` | Returns current connection state |
| `is_connected()` | `bool is_connected() const noexcept` | Returns `true` if connected |
| `server_info()` | `optional<proxy_server_info> server_info() const` | Returns server info if connected |
| `config()` | `const proxy_connection_config& config() const noexcept` | Returns current configuration |

### Connection Lifecycle

```
  ┌──────────────┐
  │ disconnected │ ←── Initial state / after shutdown()
  └──────┬───────┘
         │ initialize()
         ▼
  ┌──────────────┐
  │  connecting  │ ←── Connection attempt in progress
  └──────┬───────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
┌────────┐ ┌───────┐
│connected│ │ error │ ←── Connection failed
└────┬───┘ └───────┘
     │
     │ Query operations available
     │
     │ shutdown()
     ▼
┌──────────────┐
│ disconnected │
└──────────────┘
```

1. **Create**: Construct `proxy_connector` with database type and config
2. **Initialize**: Call `initialize()` to connect to `database_server`
3. **Query**: Use `select_query()`, `insert_query()`, etc.
4. **Shutdown**: Call `shutdown()` to disconnect cleanly
5. **Destroy**: Destructor calls `shutdown()` automatically if still connected

### Error Handling

All methods return `Result<T>` or `VoidResult` types from the common_system library:

```cpp
auto result = connector->initialize(conn_config);
if (!result.is_ok()) {
    auto error = result.error();
    // error.code: integer error code
    // error.message: descriptive error message
    // error.source: "proxy_connector"
    std::cerr << "Error [" << error.code << "]: " << error.message << std::endl;
}
```

Common error codes:
| Code | Meaning |
|------|---------|
| `-1` | General error (not connected, invalid config) |
| `-2` | Server not available / no active transaction |

### Thread Safety

- All public methods are **thread-safe**
- Internal state (`proxy_state`) uses `std::atomic<proxy_state>` for lock-free reads
- Transaction flag uses `std::atomic<bool>`
- Error message and server info are protected by `std::mutex`
- The connector is **non-copyable and non-moveable**

---

## Usage Examples

### Basic Proxy Setup with PostgreSQL

```cpp
#include "database/database_manager.h"
#include "database/proxy/proxy_config.h"

// Create database manager
auto context = std::make_shared<database::database_context>();
auto db_mgr = std::make_shared<database::database_manager>(context);

// Configure proxy
database::proxy::proxy_connection_config proxy_config;
proxy_config.server_host = "db-gateway.internal";
proxy_config.server_port = 9432;
proxy_config.auth_token = "my-client-token";

// Switch to proxy mode for PostgreSQL
if (db_mgr->set_mode_proxy(database::database_types::postgres, proxy_config)) {
    // Connect (connection string is ignored in proxy mode)
    auto result = db_mgr->connect_result("");
    if (result.is_ok()) {
        // Execute queries as usual
        auto rows = db_mgr->select_query_result("SELECT * FROM users");
        if (rows.is_ok()) {
            for (const auto& row : rows.value()) {
                // Process row data
            }
        }
        db_mgr->disconnect_result();
    }
}
```

### Proxy with TLS Configuration

```cpp
database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.example.com";
config.server_port = 9432;
config.auth_token = "secure-token";
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/db-gateway-ca.pem";

db_mgr->set_mode_proxy(database::database_types::postgres, config);
```

### Mutual TLS (mTLS) Authentication

```cpp
database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.secure.internal";
config.server_port = 9432;
config.auth_token = "mtls-client-token";
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/ca.pem";
config.client_cert_path = "/etc/ssl/certs/client.pem";
config.client_key_path = "/etc/ssl/private/client-key.pem";

db_mgr->set_mode_proxy(database::database_types::postgres, config);
```

### Fallback Strategy (Proxy to Direct)

Support both connection modes with automatic fallback:

```cpp
bool connect_with_fallback(
    std::shared_ptr<database::database_manager>& db_mgr,
    const database::proxy::proxy_connection_config& proxy_config,
    const std::string& direct_connection_string)
{
    // Try proxy mode first
    if (db_mgr->set_mode_proxy(database::database_types::postgres, proxy_config)) {
        auto result = db_mgr->connect_result("");
        if (result.is_ok()) {
            return true;  // Proxy mode connected
        }
    }

    // Fallback to direct mode
    db_mgr->set_mode(database::database_types::postgres);
    auto result = db_mgr->connect_result(direct_connection_string);
    return result.is_ok();
}
```

---

## Backend Integration

### Supported Database Backends

The proxy connector supports all database types defined in `database_types`:

| Backend | `database_types` Value | Proxy Support |
|---------|----------------------|---------------|
| PostgreSQL | `database_types::postgres` | Planned |
| MySQL | `database_types::mysql` | Planned |
| SQLite | `database_types::sqlite` | Planned |
| MongoDB | `database_types::mongodb` | Planned |
| Redis | `database_types::redis` | Planned |

The target database type is specified during `proxy_connector` construction and transmitted to the `database_server`, which routes queries to the appropriate backend.

### Proxy-Specific Limitations

- **Backend-specific SQL**: The proxy layer passes SQL strings transparently. Backend-specific syntax (e.g., PostgreSQL JSONB operators, MySQL-specific functions) is handled by the `database_server`.
- **Prepared statements**: Currently not supported through the proxy layer. Queries are sent as raw SQL strings.
- **Binary data**: Large binary objects may have additional serialization overhead through the proxy.
- **Connection-local state**: Session-level settings (e.g., `SET search_path`) may behave differently depending on the `database_server` connection pooling strategy.

---

## Performance Considerations

| Factor | Impact | Notes |
|--------|--------|-------|
| Network latency | +1 hop | Additional round-trip through `database_server` |
| Query serialization | Low | Queries serialized as strings |
| Result deserialization | Medium | Results must be deserialized from server response |
| Connection pooling | Positive | Server-side pooling can be more efficient than per-client pools |
| TLS overhead | Low-Medium | TLS handshake on initial connection; symmetric encryption on data |
| Retry mechanism | Configurable | `retry_count` and `retry_delay` control reconnection behavior |

**Recommendations**:
- Use the proxy for centralized environments with multiple client applications
- For single-application deployments with low latency requirements, direct mode may be preferable
- Configure `query_timeout` based on your longest expected query duration
- Use TLS in production; disable only for local development if needed

---

## Current Status and Roadmap

> **Current Phase**: 4.1 (Stub Implementation)
>
> All operations return `not_implemented` errors. Full functionality requires `database_server` (Phases 1-3).

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 4.1 | Complete | `proxy_connector` interface and stub implementation |
| Phase 1-3 | Not started | `database_server` middleware implementation |
| Phase 4.2 | Blocked | Network communication integration (requires Phases 1-3) |
| Phase 4.3 | Blocked | Production hardening and optimization |

```
Phase 4.1 [Complete] ─── proxy_connector interface + stub
    │
Phase 1-3 [Not started] ─── database_server implementation
    │
Phase 4.2 [Blocked] ─── Network integration
    │
Phase 4.3 [Blocked] ─── Production hardening
```

**Current recommendation**: Use DirectMode for all deployments until ProxyMode is fully implemented. See the [ProxyMode Migration Guide](../migration/proxy-mode.md) for migration planning.

---

## Related Documentation

- [ProxyMode Migration Guide](../migration/proxy-mode.md) - Step-by-step migration from direct to proxy mode
- [Architecture Overview](../ARCHITECTURE.md) - System architecture and design patterns
- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [Features Overview](../FEATURES.md) - All database_system features
