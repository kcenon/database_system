# Replication Manager API Reference

## Overview

The Replication Manager provides automated data synchronization between
database nodes with support for various replication modes and conflict resolution strategies.

## Quick Start

```cpp
#include <database/replication/replication_manager.h>

using namespace database::replication;

// Create replication manager
replication_manager replication;

// Configure source and target nodes
distributed::node_config source;
source.id = "primary";
source.host = "primary.db.local";
source.port = 5432;
source.role = distributed::node_role::PRIMARY;

distributed::node_config target;
target.id = "replica1";
target.host = "replica1.db.local";
target.port = 5432;
target.role = distributed::node_role::REPLICA;

// Configure replication
replication_config config;
config.mode = sync_mode::REALTIME;
config.conflict_resolution = conflict_strategy::LAST_WRITE_WINS;
config.batch_size = 100;

// Start replication
auto result = replication.start_replication(source, target, config);
if (result.is_ok()) {
    std::cout << "Replication started" << std::endl;
}
```

## Configuration

### replication_config

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| mode | sync_mode | REALTIME | Synchronization mode |
| conflict_resolution | conflict_strategy | LAST_WRITE_WINS | Conflict resolution strategy |
| batch_size | size_t | 100 | Changes per batch |
| batch_interval | seconds | 60 | Batch interval for BATCH mode |
| bidirectional | bool | false | Enable bidirectional replication |
| tables | vector<table_mapping> | {} | Table mapping configuration |

### sync_mode

- `REALTIME`: Replicate changes immediately as they occur
- `BATCH`: Batch changes and replicate at intervals
- `MANUAL`: Only replicate when explicitly triggered

### conflict_strategy

- `LAST_WRITE_WINS`: Use the most recent change
- `FIRST_WRITE_WINS`: Use the first change
- `MANUAL`: Require manual conflict resolution
- `CUSTOM`: Use custom conflict resolution callback

### table_mapping

| Field | Type | Description |
|-------|------|-------------|
| source_table | string | Source table name |
| target_table | string | Target table name (can differ) |
| columns | vector<string> | Specific columns to replicate |
| filter_condition | string | WHERE clause to filter rows |

## API Reference

### Lifecycle Methods

#### start_replication()
```cpp
result<void> start_replication(
    const distributed::node_config& source,
    const distributed::node_config& target,
    const replication_config& config
);
```
Starts replication between source and target nodes.

**Parameters**:
- `source`: Source node configuration
- `target`: Target node configuration
- `config`: Replication configuration

**Returns**: `result<void>` indicating success or failure.

**Example**:
```cpp
auto result = replication.start_replication(source, target, config);
if (result.is_err()) {
    std::cerr << "Failed: " << result.error().message << std::endl;
}
```

#### stop_replication()
```cpp
result<void> stop_replication();
```
Stops the active replication process.

**Returns**: `result<void>` indicating success or failure.

#### is_active()
```cpp
bool is_active() const;
```
Returns whether replication is currently active.

### Control Methods

#### pause()
```cpp
result<void> pause();
```
Pauses replication without stopping it completely.

**Example**:
```cpp
// Pause for maintenance
replication.pause();

// ... perform maintenance ...

// Resume
replication.resume();
```

#### resume()
```cpp
result<void> resume();
```
Resumes paused replication.

#### trigger_replication()
```cpp
result<void> trigger_replication();
```
Manually triggers replication (only works in MANUAL mode).

**Example**:
```cpp
replication_config config;
config.mode = sync_mode::MANUAL;

replication.start_replication(source, target, config);

// Manually trigger when ready
replication.trigger_replication();
```

### Configuration Methods

#### set_conflict_resolution()
```cpp
void set_conflict_resolution(conflict_strategy strategy);
```
Sets the conflict resolution strategy.

**Example**:
```cpp
// Switch to manual conflict resolution
replication.set_conflict_resolution(conflict_strategy::MANUAL);
```

### Monitoring Methods

#### get_replication_lag()
```cpp
std::chrono::milliseconds get_replication_lag() const;
```
Returns the current replication lag.

**Example**:
```cpp
auto lag = replication.get_replication_lag();
if (lag > std::chrono::seconds(5)) {
    std::cerr << "Warning: High replication lag: "
              << lag.count() << "ms" << std::endl;
}
```

#### get_stats()
```cpp
replication_stats get_stats() const;
```
Returns comprehensive replication statistics.

**Returns**: `replication_stats` structure containing:
- `events_replicated`: Total events replicated
- `events_failed`: Number of failed events
- `conflicts_resolved`: Number of conflicts resolved
- `current_lag`: Current replication lag
- `avg_lag`: Average replication lag
- `max_lag`: Maximum observed lag
- `last_event_time`: Time of last replicated event

**Example**:
```cpp
auto stats = replication.get_stats();
std::cout << "Replicated: " << stats.events_replicated << std::endl;
std::cout << "Failed: " << stats.events_failed << std::endl;
std::cout << "Current lag: " << stats.current_lag.count() << "ms" << std::endl;
```

#### is_healthy()
```cpp
bool is_healthy() const;
```
Returns whether replication is healthy (active and low lag).

**Example**:
```cpp
if (!replication.is_healthy()) {
    // Send alert
    alert_ops_team("Replication unhealthy");
}
```

#### get_pending_event_count()
```cpp
size_t get_pending_event_count() const;
```
Returns the number of events waiting to be replicated.

## Replication Event Structure

### replication_event

```cpp
struct replication_event {
    enum class event_type { INSERT, UPDATE, DELETE };

    event_type type;
    std::string table_name;
    std::map<std::string, std::string> old_values;  // For UPDATE/DELETE
    std::map<std::string, std::string> new_values;  // For INSERT/UPDATE
    std::chrono::system_clock::time_point timestamp;
};
```

## Error Handling

```cpp
auto result = replication.start_replication(source, target, config);
if (result.is_err()) {
    const auto& error = result.error();

    switch (error.code) {
        case -1:
            std::cerr << "Replication already active" << std::endl;
            break;
        case -2:
            std::cerr << "Replication not active" << std::endl;
            break;
        case -3:
            std::cerr << "Invalid mode for operation" << std::endl;
            break;
        case -4:
            std::cerr << "Target not initialized" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << error.message << std::endl;
    }
}
```

## Thread Safety

The Replication Manager is thread-safe. Statistics and status can be queried
from any thread while replication is active.

## Best Practices

1. **Use REALTIME mode** for low-latency requirements
2. **Use BATCH mode** for high-throughput scenarios
3. **Monitor replication lag** and alert on excessive lag (>5 seconds)
4. **Configure appropriate batch sizes** based on write volume
5. **Test failover procedures** regularly
6. **Use table mappings** for selective replication

## Configuration Examples

### High-Throughput Batch Replication

```cpp
replication_config config;
config.mode = sync_mode::BATCH;
config.batch_size = 1000;
config.batch_interval = std::chrono::seconds(30);
```

### Selective Table Replication

```cpp
replication_config config;
config.mode = sync_mode::REALTIME;

table_mapping users_mapping;
users_mapping.source_table = "users";
users_mapping.target_table = "users_replica";
users_mapping.filter_condition = "status = 'active'";
config.tables.push_back(users_mapping);

table_mapping orders_mapping;
orders_mapping.source_table = "orders";
orders_mapping.target_table = "orders";
orders_mapping.columns = {"id", "user_id", "total", "created_at"};
config.tables.push_back(orders_mapping);
```

### Manual Conflict Resolution

```cpp
replication_config config;
config.mode = sync_mode::REALTIME;
config.conflict_resolution = conflict_strategy::MANUAL;

// Conflicts will be logged for manual review
// Check logs periodically and resolve manually
```

## See Also

- [Gateway API Reference](GATEWAY_API.md)
- [Distributed Setup Guide](../guides/DISTRIBUTED_SETUP.md)
- [Cluster Manager Documentation](CLUSTER_MANAGER_API.md)
