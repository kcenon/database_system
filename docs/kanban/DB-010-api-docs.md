# DB-010: Gateway & Replication API Documentation

**Category**: DOC
**Priority**: MEDIUM
**Status**: TODO
**Est. Duration**: 3-4 days
**Dependencies**: DB-004 (Gateway), DB-005 (Replication)
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Gateway and Replication Manager APIs are defined but not documented
- No usage examples for distributed features
- No migration guide from single-node to distributed setup
- API reference documentation missing for new components

### Target State
- Complete API reference documentation for Gateway and Replication Manager
- Usage examples with code snippets
- Migration guide for distributed setup
- Integration guides for common use cases
- Troubleshooting guide for distributed issues

### Scope
**Documentation Files to Create**:
- `docs/api/GATEWAY_API.md`
- `docs/api/REPLICATION_API.md`
- `docs/guides/DISTRIBUTED_SETUP.md`
- `docs/guides/MIGRATION_TO_DISTRIBUTED.md`
- `docs/troubleshooting/DISTRIBUTED_ISSUES.md`

---

## 2. How to Change

### 2.1 Gateway API Documentation

```markdown
<!-- docs/api/GATEWAY_API.md -->
# Database Gateway API Reference

## Overview

The Database Gateway provides a centralized access point for database operations
with built-in routing, caching, and load balancing capabilities.

## Quick Start

```cpp
#include <database/distributed/database_gateway.h>

using namespace database::distributed;

// Create gateway with default configuration
gateway_config config;
config.enable_cache = true;
config.strategy = routing_strategy::ROUND_ROBIN;

database_gateway gateway(config);

// Add backend databases
backend_config backend;
backend.type = database_types::PostgreSQL;
backend.connection_string = "host=localhost;port=5432;database=mydb";
backend.role = backend_role::PRIMARY;

gateway.add_backend("primary", backend);
gateway.initialize();

// Execute queries
auto result = gateway.execute_read("SELECT * FROM users WHERE active = true");
```

## Configuration

### gateway_config

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| strategy | routing_strategy | ROUND_ROBIN | Query routing strategy |
| enable_read_write_split | bool | true | Route reads to replicas |
| enable_cache | bool | true | Enable query caching |
| cache_max_entries | size_t | 10000 | Maximum cache entries |
| cache_ttl | seconds | 300 | Cache TTL in seconds |
| max_connections_per_backend | size_t | 20 | Connection pool size |
| connection_timeout | seconds | 30 | Connection timeout |
| require_authentication | bool | false | Enable authentication |

### routing_strategy

- `ROUND_ROBIN`: Distribute queries evenly across backends
- `LEAST_CONNECTIONS`: Route to backend with fewest active connections
- `RANDOM`: Random backend selection
- `WEIGHTED`: Weight-based distribution
- `LATENCY_BASED`: Route to lowest latency backend

## API Reference

### Lifecycle Methods

#### initialize()
```cpp
bool initialize();
```
Initializes the gateway and establishes connections to all configured backends.

**Returns**: `true` if successful, `false` otherwise.

**Example**:
```cpp
if (!gateway.initialize()) {
    std::cerr << "Failed to initialize gateway" << std::endl;
    return 1;
}
```

#### shutdown()
```cpp
void shutdown();
```
Gracefully shuts down the gateway, closing all connections.

### Backend Management

#### add_backend()
```cpp
bool add_backend(const std::string& id, const backend_config& config);
```
Adds a new database backend to the gateway.

**Parameters**:
- `id`: Unique identifier for the backend
- `config`: Backend configuration

**Example**:
```cpp
backend_config replica;
replica.type = database_types::PostgreSQL;
replica.connection_string = "host=replica1;port=5432;database=mydb";
replica.role = backend_role::REPLICA;

gateway.add_backend("replica1", replica);
```

### Query Execution

#### execute_query()
```cpp
database_result execute_query(const std::string& query,
                              query_options options = {});
```
Executes a query with automatic routing based on query type.

#### execute_read()
```cpp
database_result execute_read(const std::string& query);
```
Executes a read query, potentially routed to a replica.

#### execute_write()
```cpp
database_result execute_write(const std::string& query);
```
Executes a write query, always routed to the primary.

### Cache Control

#### invalidate_cache()
```cpp
void invalidate_cache(const std::string& pattern = "*");
```
Invalidates cache entries matching the pattern.

**Example**:
```cpp
// Invalidate all user-related cache entries
gateway.invalidate_cache("*users*");

// Invalidate all cache
gateway.invalidate_cache("*");
```

## Error Handling

```cpp
try {
    auto result = gateway.execute_query("SELECT * FROM users");
    if (!result.success) {
        std::cerr << "Query failed: " << result.error_message << std::endl;
    }
} catch (const gateway_exception& e) {
    std::cerr << "Gateway error: " << e.what() << std::endl;
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
```

### 2.2 Replication API Documentation

```markdown
<!-- docs/api/REPLICATION_API.md -->
# Replication Manager API Reference

## Overview

The Replication Manager provides automated data synchronization between
database nodes with support for various replication topologies.

## Quick Start

```cpp
#include <database/distributed/replication_manager.h>

using namespace database::distributed;

// Configure replication
replication_config config;
config.topology = topology_type::PRIMARY_REPLICA;
config.mode = replication_mode::ASYNCHRONOUS;

replication_manager replication(config);

// Add nodes
replication_node primary;
primary.id = "primary";
primary.connection_string = "host=primary;port=5432;database=mydb";
primary.is_primary = true;

replication_node replica;
replica.id = "replica1";
replica.connection_string = "host=replica1;port=5432;database=mydb";
replica.is_primary = false;

replication.add_node(primary);
replication.add_node(replica);

// Start replication
replication.start();
replication.start_replication("primary", "replica1");
```

## Configuration

### replication_config

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| topology | topology_type | PRIMARY_REPLICA | Replication topology |
| mode | replication_mode | ASYNCHRONOUS | Sync mode |
| batch_size | size_t | 1000 | Changes per batch |
| batch_timeout | milliseconds | 100 | Batch timeout |
| max_retry_count | size_t | 3 | Max retry attempts |
| retry_delay | seconds | 5 | Delay between retries |
| conflict_strategy | conflict_resolution_strategy | LAST_WRITE_WINS | Conflict resolution |

### replication_mode

- `SYNCHRONOUS`: Wait for replica acknowledgment before commit
- `ASYNCHRONOUS`: Replicate in background (eventual consistency)
- `SEMI_SYNCHRONOUS`: Wait for at least one replica

### topology_type

- `PRIMARY_REPLICA`: Single primary, multiple replicas
- `MULTI_PRIMARY`: Multiple primaries with conflict resolution
- `CHAIN`: Cascading replication (A → B → C)

## API Reference

### Node Management

#### add_node()
```cpp
bool add_node(const replication_node& node);
```
Adds a node to the replication topology.

#### remove_node()
```cpp
bool remove_node(const std::string& node_id);
```
Removes a node from the topology.

#### set_primary()
```cpp
bool set_primary(const std::string& node_id);
```
Designates a node as the primary.

### Replication Control

#### start_replication()
```cpp
bool start_replication(const std::string& source_id,
                       const std::string& target_id);
```
Starts replication between two nodes.

#### stop_replication()
```cpp
bool stop_replication(const std::string& source_id,
                      const std::string& target_id);
```
Stops replication between two nodes.

### Failover

#### initiate_failover()
```cpp
bool initiate_failover(const std::string& new_primary_id);
```
Initiates failover to a new primary node.

**Example**:
```cpp
// Monitor primary health
if (!replication.is_node_healthy("primary")) {
    // Promote replica to primary
    if (replication.initiate_failover("replica1")) {
        std::cout << "Failover successful" << std::endl;
    }
}
```

### Monitoring

#### get_replication_lag()
```cpp
uint64_t get_replication_lag(const std::string& replica_id) const;
```
Returns the replication lag in bytes.

#### get_all_status()
```cpp
std::vector<replication_status> get_all_status() const;
```
Returns status for all replication streams.

## Callbacks

### on_failover()
```cpp
void on_failover(failover_callback callback);
```
Registers a callback for failover events.

**Example**:
```cpp
replication.on_failover([](const std::string& old_primary,
                           const std::string& new_primary) {
    std::cout << "Failover: " << old_primary << " -> " << new_primary << std::endl;
    // Update connection strings, notify services, etc.
});
```

### on_conflict()
```cpp
void on_conflict(conflict_callback callback);
```
Registers a custom conflict resolution callback.

## Best Practices

1. **Use asynchronous replication** for performance, synchronous for durability
2. **Monitor replication lag** and alert on excessive lag
3. **Test failover procedures** regularly
4. **Configure appropriate batch sizes** based on write volume
```

### 2.3 Migration Guide

```markdown
<!-- docs/guides/MIGRATION_TO_DISTRIBUTED.md -->
# Migration Guide: Single-Node to Distributed Setup

## Overview

This guide walks you through migrating from a single database instance
to a distributed setup using Gateway and Replication Manager.

## Prerequisites

- Database System v2.0+
- network_system dependency installed
- Multiple database instances (physical or containerized)

## Migration Steps

### Step 1: Prepare Your Existing Setup

```cpp
// Before: Direct database connection
auto db = database::create_database(database_types::PostgreSQL);
db->connect("host=localhost;port=5432;database=mydb");
auto result = db->select_query("SELECT * FROM users");
```

### Step 2: Set Up the Gateway

```cpp
// After: Gateway-based access
#include <database/distributed/database_gateway.h>

gateway_config config;
config.enable_cache = true;

database_gateway gateway(config);

// Add your existing database as primary
backend_config primary;
primary.type = database_types::PostgreSQL;
primary.connection_string = "host=localhost;port=5432;database=mydb";
primary.role = backend_role::PRIMARY;

gateway.add_backend("primary", primary);
gateway.initialize();
```

### Step 3: Add Replicas

```cpp
// Add replica nodes
backend_config replica1;
replica1.type = database_types::PostgreSQL;
replica1.connection_string = "host=replica1;port=5432;database=mydb";
replica1.role = backend_role::REPLICA;

gateway.add_backend("replica1", replica1);
```

### Step 4: Enable Replication

```cpp
#include <database/distributed/replication_manager.h>

replication_config rep_config;
rep_config.mode = replication_mode::ASYNCHRONOUS;

replication_manager replication(rep_config);
// ... configure nodes ...
replication.start_replication("primary", "replica1");
```

### Step 5: Update Application Code

```cpp
// Replace direct DB calls with gateway calls
// Before:
auto result = db->select_query("SELECT * FROM users");

// After:
auto result = gateway.execute_read("SELECT * FROM users");
```

## Rollback Plan

If issues occur, revert to direct database connections:

1. Stop replication
2. Bypass gateway
3. Connect directly to primary database

## Checklist

- [ ] Database instances provisioned
- [ ] Network connectivity verified
- [ ] Gateway configured and tested
- [ ] Replication started and verified
- [ ] Application code updated
- [ ] Monitoring configured
- [ ] Failover tested
```

### 2.4 Implementation Steps

1. **API Reference Documentation** (Days 1-2)
   - Gateway API complete reference
   - Replication Manager API complete reference
   - Code examples for all methods

2. **Guides and Tutorials** (Day 2-3)
   - Distributed setup guide
   - Migration guide
   - Integration patterns

3. **Troubleshooting Documentation** (Day 3-4)
   - Common issues and solutions
   - Debug procedures
   - Performance troubleshooting

4. **Review and Polish** (Day 4)
   - Technical review
   - Example code verification
   - Cross-reference links

---

## 3. How to Test

### 3.1 Documentation Verification

```bash
# Check markdown syntax
markdownlint docs/api/*.md docs/guides/*.md

# Verify internal links
markdown-link-check docs/**/*.md

# Spell check
aspell check docs/**/*.md
```

### 3.2 Code Example Verification

```bash
# Extract and compile code examples
./scripts/extract_doc_examples.sh
cmake -B build_examples
cmake --build build_examples
./build_examples/doc_examples
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| All public APIs documented | 100% | Manual review |
| Code examples compile | Yes | CI build |
| Internal links valid | 100% | Link checker |
| Spelling errors | 0 | Spell check |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| API changes before docs complete | HIGH | Coordinate with feature tickets |
| Examples become outdated | MEDIUM | Automated example testing |
| Missing edge cases | LOW | User feedback loop |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**:
  - [DB-004](DB-004-gateway.md) (Gateway)
  - [DB-005](DB-005-replication.md) (Replication)
- **Related**:
  - [DB-015](DB-015-korean-docs.md) (Korean Documentation)

---

## 6. Notes

- Keep documentation updated as implementation evolves
- Include diagrams for complex concepts
- Add "See Also" sections for related topics
- Consider generating API docs from code comments (Doxygen)

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
