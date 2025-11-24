# DB-004: Implement Database Gateway

**Category**: FEATURE
**Priority**: HIGH
**Status**: TODO
**Est. Duration**: 10-14 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Database Gateway API is defined but implementation is pending (marked as 🚧 in DISTRIBUTED_FEATURES.md)
- Currently no centralized database access point for microservices
- No query routing, caching, or security layer at the gateway level
- Direct database connections required from each service

### Target State
- Fully functional Database Gateway implementation
- Centralized query routing with load balancing
- Built-in query caching layer
- Connection pooling at the gateway level
- Security and authentication middleware
- Protocol translation between different database types

### Scope
**Target Files**:
- `database/distributed/database_gateway.h` (new or existing API)
- `database/distributed/database_gateway.cpp` (implementation)
- `database/distributed/gateway_router.h/cpp` (query routing)
- `database/distributed/gateway_cache.h/cpp` (caching layer)

**Architecture Components**:
```
┌─────────────────────────────────────────────────────────────┐
│                    DATABASE GATEWAY                          │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   Router    │  │    Cache    │  │   Security  │         │
│  │  (Routing)  │  │   (Redis)   │  │   (Auth)    │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         └────────────────┼────────────────┘                 │
│                          │                                  │
│  ┌───────────────────────┴───────────────────────┐         │
│  │              Connection Manager                │         │
│  └───────────────────────┬───────────────────────┘         │
├──────────────────────────┼──────────────────────────────────┤
│      ┌───────────────────┼───────────────────┐              │
│      │                   │                   │              │
│      ▼                   ▼                   ▼              │
│ ┌─────────┐        ┌─────────┐        ┌─────────┐          │
│ │PostgreSQL│        │  MySQL  │        │ MongoDB │          │
│ └─────────┘        └─────────┘        └─────────┘          │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. How to Change

### 2.1 Gateway Core Interface

```cpp
// database/distributed/database_gateway.h
#pragma once

#include "gateway_config.h"
#include "gateway_router.h"
#include "gateway_cache.h"
#include "../database_base.h"
#include <memory>
#include <string>
#include <functional>

namespace database::distributed {

/**
 * @brief Configuration for the Database Gateway
 */
struct gateway_config {
    // Routing configuration
    routing_strategy strategy = routing_strategy::ROUND_ROBIN;
    bool enable_read_write_split = true;

    // Cache configuration
    bool enable_cache = true;
    size_t cache_max_entries = 10000;
    std::chrono::seconds cache_ttl{300};

    // Connection configuration
    size_t max_connections_per_backend = 20;
    std::chrono::seconds connection_timeout{30};

    // Security configuration
    bool require_authentication = false;
    std::string auth_secret_key;
};

/**
 * @brief Central gateway for database access
 *
 * Provides unified access to multiple database backends with:
 * - Query routing and load balancing
 * - Query caching
 * - Connection pooling
 * - Security middleware
 */
class database_gateway {
public:
    explicit database_gateway(const gateway_config& config = {});
    ~database_gateway();

    // Lifecycle
    bool initialize();
    void shutdown();
    bool is_running() const;

    // Backend Management
    bool add_backend(const std::string& id, const backend_config& config);
    bool remove_backend(const std::string& id);
    std::vector<std::string> list_backends() const;

    // Query Execution
    database_result execute_query(const std::string& query,
                                  query_options options = {});
    database_result execute_read(const std::string& query);
    database_result execute_write(const std::string& query);

    // Transaction Support
    transaction_handle begin_transaction();
    bool commit_transaction(transaction_handle& txn);
    bool rollback_transaction(transaction_handle& txn);

    // Cache Control
    void invalidate_cache(const std::string& pattern = "*");
    cache_stats get_cache_stats() const;

    // Monitoring
    gateway_stats get_stats() const;
    std::vector<backend_status> get_backend_health() const;

    // Middleware
    void add_middleware(std::function<void(query_context&)> middleware);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace database::distributed
```

### 2.2 Router Implementation

```cpp
// database/distributed/gateway_router.h
#pragma once

#include <vector>
#include <string>
#include <memory>

namespace database::distributed {

enum class routing_strategy {
    ROUND_ROBIN,
    LEAST_CONNECTIONS,
    RANDOM,
    WEIGHTED,
    LATENCY_BASED
};

enum class query_type {
    READ,
    WRITE,
    DDL,
    TRANSACTION
};

class gateway_router {
public:
    explicit gateway_router(routing_strategy strategy);

    // Route selection
    std::string select_backend(query_type type,
                               const std::vector<backend_info>& backends);

    // Strategy management
    void set_strategy(routing_strategy strategy);
    routing_strategy get_strategy() const;

    // Query analysis
    static query_type analyze_query(const std::string& query);

private:
    routing_strategy strategy_;
    std::atomic<size_t> round_robin_index_{0};

    std::string select_round_robin(const std::vector<backend_info>& backends);
    std::string select_least_connections(const std::vector<backend_info>& backends);
    std::string select_by_latency(const std::vector<backend_info>& backends);
};

} // namespace database::distributed
```

### 2.3 Cache Implementation

```cpp
// database/distributed/gateway_cache.h
#pragma once

#include <string>
#include <chrono>
#include <optional>
#include "../database_types.h"

namespace database::distributed {

struct cache_entry {
    database_result data;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point expires_at;
    size_t hit_count = 0;
};

struct cache_stats {
    size_t total_entries;
    size_t hit_count;
    size_t miss_count;
    double hit_rate;
    size_t memory_usage_bytes;
};

class gateway_cache {
public:
    explicit gateway_cache(size_t max_entries,
                          std::chrono::seconds default_ttl);
    ~gateway_cache();

    // Cache operations
    std::optional<database_result> get(const std::string& query_hash);
    void put(const std::string& query_hash,
             const database_result& result,
             std::chrono::seconds ttl = {});

    // Cache management
    void invalidate(const std::string& pattern);
    void clear();
    void prune_expired();

    // Statistics
    cache_stats get_stats() const;

private:
    class impl;
    std::unique_ptr<impl> impl_;

    std::string compute_hash(const std::string& query) const;
    bool matches_pattern(const std::string& key, const std::string& pattern) const;
};

} // namespace database::distributed
```

### 2.4 Implementation Steps

1. **Core Gateway Structure** (Days 1-3)
   - Implement `database_gateway` class skeleton
   - Backend registration and management
   - Basic query forwarding without routing

2. **Router Implementation** (Days 4-6)
   - Query type analysis (READ/WRITE detection)
   - Round-robin routing
   - Least-connections routing
   - Read/write splitting

3. **Cache Layer** (Days 7-9)
   - In-memory LRU cache implementation
   - Cache key generation (query hashing)
   - TTL and invalidation support
   - Cache statistics

4. **Integration & Testing** (Days 10-14)
   - Integration with existing cluster_manager
   - End-to-end testing
   - Performance benchmarking
   - Documentation

---

## 3. How to Test

### 3.1 Unit Tests

```cpp
// tests/gateway_test.cpp
#include <gtest/gtest.h>
#include "database/distributed/database_gateway.h"

class GatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        gateway_config config;
        config.enable_cache = true;
        gateway_ = std::make_unique<database_gateway>(config);
    }

    std::unique_ptr<database_gateway> gateway_;
};

// Basic functionality
TEST_F(GatewayTest, InitializeAndShutdown) {
    EXPECT_TRUE(gateway_->initialize());
    EXPECT_TRUE(gateway_->is_running());
    gateway_->shutdown();
    EXPECT_FALSE(gateway_->is_running());
}

// Backend management
TEST_F(GatewayTest, AddAndRemoveBackend) {
    gateway_->initialize();

    backend_config backend;
    backend.type = database_types::SQLite;
    backend.connection_string = ":memory:";

    EXPECT_TRUE(gateway_->add_backend("sqlite1", backend));
    EXPECT_EQ(gateway_->list_backends().size(), 1);

    EXPECT_TRUE(gateway_->remove_backend("sqlite1"));
    EXPECT_TRUE(gateway_->list_backends().empty());
}

// Query routing
TEST_F(GatewayTest, ReadWriteSplitting) {
    // Setup primary and replica backends
    // ...

    // Write should go to primary
    gateway_->execute_write("INSERT INTO test VALUES (1)");

    // Read should go to replica
    auto result = gateway_->execute_read("SELECT * FROM test");

    // Verify routing through backend statistics
}

// Cache functionality
TEST_F(GatewayTest, QueryCaching) {
    gateway_->initialize();
    // Add backend...

    // First query - cache miss
    auto result1 = gateway_->execute_read("SELECT * FROM users");

    // Second identical query - cache hit
    auto result2 = gateway_->execute_read("SELECT * FROM users");

    auto stats = gateway_->get_cache_stats();
    EXPECT_EQ(stats.hit_count, 1);
    EXPECT_EQ(stats.miss_count, 1);
}
```

### 3.2 Integration Tests

```cpp
// tests/gateway_integration_test.cpp
TEST(GatewayIntegration, MultiBackendRouting) {
    // Test with multiple real database backends
}

TEST(GatewayIntegration, FailoverScenario) {
    // Test failover when primary backend fails
}

TEST(GatewayIntegration, TransactionAcrossGateway) {
    // Test transaction handling through gateway
}
```

### 3.3 Test Execution

```bash
# Run gateway tests
ctest -R gateway -V

# Run with integration tests (requires Docker)
docker-compose -f docker-compose.test.yml up -d
ctest -R gateway_integration -V
```

### 3.4 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Unit test coverage | 80%+ | gcovr report |
| Query routing accuracy | 100% | Unit tests |
| Cache hit rate (repeated queries) | >90% | Benchmark |
| Failover time | <5 seconds | Integration test |
| Throughput (single backend) | >5000 QPS | Benchmark |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Cache inconsistency | HIGH | TTL-based expiration, write-through invalidation |
| Single point of failure | HIGH | Support gateway replication |
| Performance overhead | MEDIUM | Connection pooling, async I/O |
| Complex transaction handling | MEDIUM | Document limitations, single-backend transactions |

---

## 5. Related Tickets

- **Blocks**:
  - [DB-005](DB-005-replication.md) (Replication Manager)
  - [DB-010](DB-010-api-docs.md) (API Documentation)
  - [DB-011](DB-011-multi-node.md) (Multi-Node Tests)
- **Blocked by**: None
- **Related**:
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)

---

## 6. Notes

- Gateway should integrate with existing `cluster_manager` from DISTRIBUTED_FEATURES.md
- Consider using `network_system` for inter-service communication
- Cache invalidation strategy critical for data consistency
- Transaction support limited to single-backend operations initially

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
