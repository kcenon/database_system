# DB-005: Implement Replication Manager

**Category**: FEATURE
**Priority**: HIGH
**Status**: DONE
**Est. Duration**: 12-16 days
**Dependencies**: DB-004 (Gateway)
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Replication Manager API is defined but implementation is pending (marked as 🚧 in DISTRIBUTED_FEATURES.md)
- No automated data replication between database instances
- Manual data migration required for disaster recovery
- No support for real-time data synchronization

### Target State
- Fully functional Replication Manager implementation
- Support for primary-replica replication topologies
- Real-time change data capture (CDC)
- Automated failover with replica promotion
- Cross-database type replication support
- Configurable replication strategies (sync/async)

### Scope
**Target Files**:
- `database/distributed/replication_manager.h` (new or existing API)
- `database/distributed/replication_manager.cpp` (implementation)
- `database/distributed/change_stream.h/cpp` (CDC)
- `database/distributed/replication_log.h/cpp` (WAL handling)

**Architecture Components**:
```
┌─────────────────────────────────────────────────────────────────┐
│                    REPLICATION MANAGER                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐    │
│  │ Change Data  │────▶│ Replication  │────▶│   Conflict   │    │
│  │   Capture    │     │     Log      │     │   Resolver   │    │
│  └──────────────┘     └──────────────┘     └──────────────┘    │
│         │                    │                    │             │
│         ▼                    ▼                    ▼             │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Replication Engine                     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
├──────────────────────────────┼──────────────────────────────────┤
│         ┌────────────────────┼────────────────────┐             │
│         │                    │                    │             │
│         ▼                    ▼                    ▼             │
│   ┌──────────┐         ┌──────────┐         ┌──────────┐       │
│   │ PRIMARY  │         │ REPLICA  │         │ REPLICA  │       │
│   │(PostgreSQL)│        │  (MySQL) │         │(PostgreSQL)│      │
│   └──────────┘         └──────────┘         └──────────┘       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. How to Change

### 2.1 Replication Manager Interface

```cpp
// database/distributed/replication_manager.h
#pragma once

#include "replication_config.h"
#include "change_stream.h"
#include "../database_base.h"
#include <memory>
#include <functional>

namespace database::distributed {

/**
 * @brief Replication mode for data synchronization
 */
enum class replication_mode {
    SYNCHRONOUS,      // Wait for replica acknowledgment
    ASYNCHRONOUS,     // Fire and forget
    SEMI_SYNCHRONOUS  // Wait for at least one replica
};

/**
 * @brief Replication topology type
 */
enum class topology_type {
    PRIMARY_REPLICA,  // One primary, multiple replicas
    MULTI_PRIMARY,    // Multiple primaries (conflict resolution needed)
    CHAIN            // A -> B -> C cascading replication
};

/**
 * @brief Configuration for replication
 */
struct replication_config {
    topology_type topology = topology_type::PRIMARY_REPLICA;
    replication_mode mode = replication_mode::ASYNCHRONOUS;

    // Performance tuning
    size_t batch_size = 1000;
    std::chrono::milliseconds batch_timeout{100};

    // Reliability
    size_t max_retry_count = 3;
    std::chrono::seconds retry_delay{5};

    // Conflict resolution (for multi-primary)
    conflict_resolution_strategy conflict_strategy =
        conflict_resolution_strategy::LAST_WRITE_WINS;
};

/**
 * @brief Node role in replication topology
 */
struct replication_node {
    std::string id;
    std::string connection_string;
    database_types db_type;
    bool is_primary;
    int priority = 0;  // For failover ordering
};

/**
 * @brief Status of a replication stream
 */
struct replication_status {
    std::string source_id;
    std::string target_id;
    uint64_t lag_bytes;
    uint64_t lag_transactions;
    std::chrono::milliseconds lag_time;
    bool is_healthy;
    std::string last_error;
};

/**
 * @brief Manages data replication across database nodes
 */
class replication_manager {
public:
    explicit replication_manager(const replication_config& config = {});
    ~replication_manager();

    // Lifecycle
    bool start();
    void stop();
    bool is_running() const;

    // Node Management
    bool add_node(const replication_node& node);
    bool remove_node(const std::string& node_id);
    bool set_primary(const std::string& node_id);
    std::vector<replication_node> get_nodes() const;

    // Replication Control
    bool start_replication(const std::string& source_id,
                          const std::string& target_id);
    bool stop_replication(const std::string& source_id,
                         const std::string& target_id);
    bool pause_replication(const std::string& stream_id);
    bool resume_replication(const std::string& stream_id);

    // Failover
    bool initiate_failover(const std::string& new_primary_id);
    bool promote_replica(const std::string& replica_id);
    std::string get_current_primary() const;

    // Monitoring
    std::vector<replication_status> get_all_status() const;
    replication_status get_stream_status(const std::string& source_id,
                                        const std::string& target_id) const;
    uint64_t get_replication_lag(const std::string& replica_id) const;

    // Callbacks
    using failover_callback = std::function<void(const std::string& old_primary,
                                                  const std::string& new_primary)>;
    void on_failover(failover_callback callback);

    using conflict_callback = std::function<database_row(const database_row& local,
                                                          const database_row& remote)>;
    void on_conflict(conflict_callback callback);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace database::distributed
```

### 2.2 Change Data Capture

```cpp
// database/distributed/change_stream.h
#pragma once

#include <string>
#include <functional>
#include <variant>
#include "../database_types.h"

namespace database::distributed {

enum class change_type {
    INSERT,
    UPDATE,
    DELETE,
    SCHEMA_CHANGE
};

struct change_event {
    change_type type;
    std::string table_name;
    std::string schema_name;
    database_row old_values;  // For UPDATE/DELETE
    database_row new_values;  // For INSERT/UPDATE
    uint64_t sequence_number;
    std::chrono::system_clock::time_point timestamp;
    std::string transaction_id;
};

class change_stream {
public:
    explicit change_stream(database_base* source);
    ~change_stream();

    // Stream control
    bool start(const std::vector<std::string>& tables = {});
    void stop();

    // Event handling
    using change_handler = std::function<void(const change_event&)>;
    void subscribe(change_handler handler);

    // Position management
    uint64_t get_current_position() const;
    bool seek_to_position(uint64_t position);
    bool seek_to_timestamp(std::chrono::system_clock::time_point ts);

    // Batching
    std::vector<change_event> poll(size_t max_events,
                                   std::chrono::milliseconds timeout);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace database::distributed
```

### 2.3 Replication Engine

```cpp
// database/distributed/replication_engine.cpp (implementation snippet)

void replication_engine::apply_changes(const std::vector<change_event>& events,
                                       database_base* target) {
    for (const auto& event : events) {
        switch (event.type) {
            case change_type::INSERT:
                apply_insert(event, target);
                break;
            case change_type::UPDATE:
                apply_update(event, target);
                break;
            case change_type::DELETE:
                apply_delete(event, target);
                break;
            case change_type::SCHEMA_CHANGE:
                apply_schema_change(event, target);
                break;
        }
    }
}

void replication_engine::apply_insert(const change_event& event,
                                      database_base* target) {
    sql_query_builder builder;
    builder.insert_into(event.table_name)
           .values(convert_to_map(event.new_values));

    auto query = builder.build_for_database(target->database_type());
    target->insert_query(query);
}

conflict_resolution_result replication_engine::resolve_conflict(
    const change_event& local,
    const change_event& remote) {

    switch (config_.conflict_strategy) {
        case conflict_resolution_strategy::LAST_WRITE_WINS:
            return local.timestamp > remote.timestamp ? local : remote;

        case conflict_resolution_strategy::FIRST_WRITE_WINS:
            return local.timestamp < remote.timestamp ? local : remote;

        case conflict_resolution_strategy::CUSTOM:
            if (conflict_callback_) {
                return conflict_callback_(local, remote);
            }
            [[fallthrough]];

        default:
            return local;  // Default to local
    }
}
```

### 2.4 Implementation Steps

1. **Core Infrastructure** (Days 1-4)
   - Implement `replication_manager` class skeleton
   - Node registration and topology management
   - Basic health monitoring

2. **Change Data Capture** (Days 5-8)
   - PostgreSQL WAL reader implementation
   - MySQL binlog reader implementation
   - SQLite trigger-based CDC
   - Unified change event format

3. **Replication Engine** (Days 9-12)
   - Event serialization and transport
   - Change application logic
   - Conflict detection and resolution
   - Transaction ordering

4. **Failover & Recovery** (Days 13-14)
   - Automatic failover detection
   - Replica promotion
   - Replication catchup
   - Split-brain prevention

5. **Testing & Documentation** (Days 15-16)
   - Unit and integration tests
   - Chaos testing (network partitions)
   - API documentation

---

## 3. How to Test

### 3.1 Unit Tests

```cpp
// tests/replication_test.cpp
#include <gtest/gtest.h>
#include "database/distributed/replication_manager.h"

class ReplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        replication_config config;
        config.mode = replication_mode::ASYNCHRONOUS;
        manager_ = std::make_unique<replication_manager>(config);
    }

    std::unique_ptr<replication_manager> manager_;
};

TEST_F(ReplicationTest, AddAndRemoveNodes) {
    replication_node primary;
    primary.id = "primary";
    primary.is_primary = true;
    primary.db_type = database_types::SQLite;
    primary.connection_string = ":memory:";

    EXPECT_TRUE(manager_->add_node(primary));
    EXPECT_EQ(manager_->get_nodes().size(), 1);

    EXPECT_TRUE(manager_->remove_node("primary"));
    EXPECT_TRUE(manager_->get_nodes().empty());
}

TEST_F(ReplicationTest, StartReplication) {
    // Add primary and replica nodes
    // Start replication
    // Verify status
}

TEST_F(ReplicationTest, FailoverScenario) {
    // Setup primary-replica topology
    // Simulate primary failure
    // Verify automatic failover
    // Verify new primary election
}
```

### 3.2 Integration Tests

```cpp
// tests/replication_integration_test.cpp

TEST(ReplicationIntegration, DataSynchronization) {
    // Setup: Primary with SQLite, Replica with SQLite
    // Action: Insert data on primary
    // Verify: Data appears on replica
}

TEST(ReplicationIntegration, CrossDatabaseReplication) {
    // Setup: PostgreSQL primary, MySQL replica
    // Verify: Schema and data type translation
}

TEST(ReplicationIntegration, ConflictResolution) {
    // Setup: Multi-primary topology
    // Action: Concurrent writes to same row
    // Verify: Conflict resolved correctly
}
```

### 3.3 Chaos Testing

```bash
# Network partition test
docker network disconnect db_network replica_container
sleep 30
docker network connect db_network replica_container
# Verify replication catches up

# Primary failure test
docker stop primary_container
# Verify failover occurs
docker start primary_container
# Verify old primary rejoins as replica
```

### 3.4 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Replication lag (async) | <1 second | Monitoring metrics |
| Failover time | <30 seconds | Integration test |
| Data consistency | 100% | Checksum comparison |
| Cross-DB replication | PostgreSQL ↔ MySQL | Integration test |
| Conflict resolution | Configurable | Unit test |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Data loss during failover | CRITICAL | Synchronous mode option |
| Split-brain scenario | CRITICAL | Fencing and quorum |
| Schema drift | HIGH | DDL replication support |
| Performance impact | MEDIUM | Async replication default |
| Network partitions | HIGH | Catchup and reconciliation |

---

## 5. Related Tickets

- **Blocks**:
  - [DB-010](DB-010-api-docs.md) (API Documentation)
  - [DB-011](DB-011-multi-node.md) (Multi-Node Tests)
- **Blocked by**:
  - [DB-004](DB-004-gateway.md) (Gateway Implementation)
- **Related**:
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)

---

## 6. Notes

- CDC implementation varies significantly by database type
  - PostgreSQL: Use logical replication slots
  - MySQL: Use binlog streaming
  - SQLite: Use triggers or WAL
- Initial implementation focuses on Primary-Replica topology
- Multi-primary support is Phase 2
- Consider using existing CDC tools (Debezium patterns) as reference

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
