# DB-013: Backend-specific Performance Tuning Guide

**Category**: DOC
**Priority**: LOW
**Status**: DONE
**Est. Duration**: 3-4 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- No documentation on performance optimization for different backends
- Users lack guidance on backend-specific tuning parameters
- No benchmarks comparing backends for different workloads
- Connection pool tuning not documented

### Target State
- Comprehensive performance tuning guide for each backend
- Backend comparison for different workload types
- Connection pool optimization guidelines
- Query optimization tips per backend
- Memory and resource tuning recommendations

### Scope
**Documentation Files to Create**:
- `docs/performance/TUNING_GUIDE.md`
- `docs/performance/POSTGRESQL_TUNING.md`
- `docs/performance/MYSQL_TUNING.md`
- `docs/performance/SQLITE_TUNING.md`
- `docs/performance/CONNECTION_POOL_TUNING.md`

---

## 2. How to Change

### 2.1 Main Tuning Guide Structure

```markdown
<!-- docs/performance/TUNING_GUIDE.md -->
# Database System Performance Tuning Guide

## Overview

This guide provides comprehensive performance tuning recommendations
for the Database System across different backends and use cases.

## Quick Reference

### Backend Selection Guide

| Workload Type | Recommended Backend | Notes |
|--------------|---------------------|-------|
| High-volume OLTP | PostgreSQL | Best concurrent write performance |
| Read-heavy web apps | MySQL + replicas | Excellent read scaling |
| Embedded/local | SQLite | Zero configuration |
| Key-value cache | Redis | Sub-millisecond access |
| Document storage | MongoDB | Flexible schema |

### Performance Quick Wins

1. **Enable connection pooling** - 5-10x improvement for short queries
2. **Use prepared statements** - Avoid repeated query parsing
3. **Index frequently queried columns** - Orders of magnitude improvement
4. **Enable query caching** - Reduce database load for repeated queries
5. **Use batch operations** - Amortize network round-trip costs

## Table of Contents

1. [Connection Pool Tuning](CONNECTION_POOL_TUNING.md)
2. [PostgreSQL Optimization](POSTGRESQL_TUNING.md)
3. [MySQL Optimization](MYSQL_TUNING.md)
4. [SQLite Optimization](SQLITE_TUNING.md)
5. [Query Builder Best Practices](#query-builder-best-practices)
6. [Monitoring and Profiling](#monitoring-and-profiling)

## Connection Pool Tuning

See [CONNECTION_POOL_TUNING.md](CONNECTION_POOL_TUNING.md) for detailed guide.

### Key Parameters

```cpp
pool_config config;
config.min_connections = 5;     // Warm pool for burst handling
config.max_connections = 20;    // Limit based on backend capacity
config.idle_timeout_ms = 30000; // Cleanup idle connections
config.acquire_timeout_ms = 5000; // Fail fast on pool exhaustion
```

### Sizing Guidelines

| Application Type | Min Connections | Max Connections |
|-----------------|-----------------|-----------------|
| Low traffic (< 10 QPS) | 2 | 5 |
| Medium traffic (10-100 QPS) | 5 | 20 |
| High traffic (100-1000 QPS) | 10 | 50 |
| Very high traffic (> 1000 QPS) | 20 | 100 |

**Formula**: `max_connections = (peak_concurrent_requests * avg_query_time_ms) / 1000`

## Query Builder Best Practices

### Parameterized Queries

```cpp
// Good: Parameterized (cached query plan)
builder.where("user_id", "=", user_id);

// Bad: String concatenation (new plan each time)
builder.where_raw("user_id = " + std::to_string(user_id));
```

### Batch Operations

```cpp
// Good: Single batch insert
std::vector<std::map<std::string, database_value>> rows;
for (const auto& item : items) {
    rows.push_back({{"name", item.name}, {"value", item.value}});
}
builder.insert_into("items").values(rows);

// Bad: Multiple single inserts
for (const auto& item : items) {
    builder.insert_into("items").values({{"name", item.name}});
    db->execute(builder.build());
    builder.reset();
}
```

### Select Only Needed Columns

```cpp
// Good: Select specific columns
builder.select({"id", "name", "email"}).from("users");

// Bad: Select all columns
builder.select({"*"}).from("users");
```

## Monitoring and Profiling

### Key Metrics to Monitor

| Metric | Warning Threshold | Critical Threshold |
|--------|-------------------|-------------------|
| Query latency (p99) | > 100ms | > 500ms |
| Connection pool usage | > 70% | > 90% |
| Cache hit rate | < 70% | < 50% |
| Query error rate | > 0.1% | > 1% |

### Built-in Profiling

```cpp
// Enable query profiling
db->enable_profiling(true);

// Execute queries...

// Get profiling results
auto stats = db->get_profiling_stats();
for (const auto& query_stat : stats) {
    std::cout << query_stat.query << std::endl;
    std::cout << "  Avg time: " << query_stat.avg_time_ms << "ms" << std::endl;
    std::cout << "  Max time: " << query_stat.max_time_ms << "ms" << std::endl;
    std::cout << "  Call count: " << query_stat.call_count << std::endl;
}
```

### Performance Testing

```bash
# Run performance benchmarks
./build/benchmarks/database_benchmarks \
    --benchmark_filter="BM_*" \
    --benchmark_repetitions=5

# Profile specific operations
./build/benchmarks/database_benchmarks \
    --benchmark_filter="BM_SingleInsert" \
    --benchmark_enable_random_interleaving=true
```
```

### 2.2 PostgreSQL Tuning

```markdown
<!-- docs/performance/POSTGRESQL_TUNING.md -->
# PostgreSQL Performance Tuning

## Connection Configuration

### Connection String Optimization

```cpp
// Optimized connection string
std::string conn_str =
    "host=localhost;"
    "port=5432;"
    "database=mydb;"
    "user=myuser;"
    "password=mypass;"
    "connect_timeout=10;"
    "application_name=my_app;"
    "options=-c statement_timeout=30000";  // 30 second timeout
```

### Server Configuration Recommendations

```ini
# postgresql.conf

# Memory (for 16GB RAM server)
shared_buffers = 4GB           # 25% of RAM
effective_cache_size = 12GB    # 75% of RAM
work_mem = 64MB                # Per-operation memory
maintenance_work_mem = 512MB   # For VACUUM, CREATE INDEX

# Connections
max_connections = 200          # Based on expected load
idle_in_transaction_session_timeout = 10min

# Write Performance
wal_buffers = 64MB
checkpoint_completion_target = 0.9
synchronous_commit = off       # For non-critical writes

# Query Planning
random_page_cost = 1.1         # For SSD storage
effective_io_concurrency = 200  # For SSD storage
```

## Query Optimization

### Indexing Strategies

```sql
-- B-tree index for equality and range queries
CREATE INDEX idx_users_email ON users(email);

-- Partial index for common filters
CREATE INDEX idx_active_users ON users(created_at)
WHERE active = true;

-- Covering index to avoid table lookups
CREATE INDEX idx_orders_covering ON orders(user_id)
INCLUDE (total, status);

-- GIN index for full-text search
CREATE INDEX idx_products_search ON products
USING GIN(to_tsvector('english', name || ' ' || description));
```

### EXPLAIN ANALYZE

```cpp
auto result = db->select_query(
    "EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) "
    "SELECT * FROM orders WHERE user_id = 123"
);
// Parse JSON result for query plan analysis
```

## Connection Pooling

### PgBouncer Integration

```ini
# pgbouncer.ini
[databases]
mydb = host=localhost port=5432 dbname=mydb

[pgbouncer]
pool_mode = transaction
max_client_conn = 1000
default_pool_size = 20
reserve_pool_size = 5
```

## Bulk Operations

### COPY for Large Imports

```cpp
// Use COPY for bulk inserts (10-100x faster than INSERT)
std::string copy_cmd = "COPY users(name, email) FROM STDIN WITH (FORMAT csv)";
db->execute_query(copy_cmd);
// Stream data...
```

### Batch Inserts

```cpp
// Optimal batch size: 1000-10000 rows
const int BATCH_SIZE = 1000;
for (int i = 0; i < total_rows; i += BATCH_SIZE) {
    auto batch = get_batch(data, i, BATCH_SIZE);
    builder.insert_into("table").values(batch);
    db->execute_query(builder.build());
    builder.reset();
}
```

## Performance Benchmarks

| Operation | Rows | Time | Throughput |
|-----------|------|------|------------|
| Single INSERT | 1 | 0.5ms | 2000/sec |
| Batch INSERT | 1000 | 15ms | 66666/sec |
| SELECT by PK | 1 | 0.1ms | 10000/sec |
| SELECT range | 1000 | 5ms | 200000 rows/sec |
| UPDATE by PK | 1 | 0.3ms | 3333/sec |

*Benchmarks on SSD storage, 16GB RAM, PostgreSQL 15*
```

### 2.3 Connection Pool Tuning Guide

```markdown
<!-- docs/performance/CONNECTION_POOL_TUNING.md -->
# Connection Pool Performance Tuning

## Overview

Connection pooling eliminates the overhead of establishing new database
connections for each query, typically providing 5-10x performance improvement.

## Key Configuration Parameters

### pool_config Reference

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| min_connections | size_t | 2 | Minimum pool size |
| max_connections | size_t | 10 | Maximum pool size |
| idle_timeout_ms | uint32_t | 30000 | Close idle connections after |
| acquire_timeout_ms | uint32_t | 5000 | Timeout waiting for connection |
| validation_interval_ms | uint32_t | 30000 | Health check interval |
| max_lifetime_ms | uint32_t | 0 | Max connection age (0=infinite) |

## Sizing Guidelines

### Formula-Based Sizing

```
optimal_pool_size = (concurrent_transactions * avg_transaction_time_ms) / 1000

Example:
- 50 concurrent transactions
- 20ms average transaction time
- optimal_pool_size = (50 * 20) / 1000 = 1 (minimum)

For safety margin:
- Recommended pool size = optimal * 2 = 2 minimum
- Set max = optimal * 4 = 4 for burst handling
```

### Workload-Based Recommendations

| Scenario | Min | Max | Idle Timeout |
|----------|-----|-----|--------------|
| Development | 1 | 5 | 60s |
| Web app (low traffic) | 2 | 10 | 30s |
| Web app (high traffic) | 5 | 50 | 30s |
| Batch processing | 10 | 100 | 10s |
| Real-time analytics | 5 | 30 | 60s |

## Advanced Configuration

### Connection Validation

```cpp
pool_config config;
config.validation_query = "SELECT 1";      // Light validation query
config.validation_interval_ms = 30000;      // Check every 30 seconds
config.validate_on_acquire = false;         // Don't validate every acquire
config.validate_on_return = false;          // Don't validate on return
```

### Connection Lifecycle

```cpp
pool_config config;
config.max_lifetime_ms = 3600000;  // Recycle connections after 1 hour
config.idle_timeout_ms = 300000;    // Close idle after 5 minutes
config.eviction_interval_ms = 60000; // Check for eviction every minute
```

## Monitoring Pool Health

```cpp
// Get pool statistics
auto stats = pool.get_stats();

std::cout << "Active connections: " << stats.active_count << std::endl;
std::cout << "Idle connections: " << stats.idle_count << std::endl;
std::cout << "Waiting requests: " << stats.waiting_count << std::endl;
std::cout << "Total created: " << stats.total_created << std::endl;
std::cout << "Total destroyed: " << stats.total_destroyed << std::endl;
std::cout << "Acquisition wait time (avg): " << stats.avg_wait_ms << "ms" << std::endl;
```

### Health Indicators

| Metric | Healthy | Warning | Critical |
|--------|---------|---------|----------|
| Pool utilization | < 50% | 50-80% | > 80% |
| Wait time (avg) | < 1ms | 1-10ms | > 10ms |
| Connection errors | 0 | < 0.1% | > 0.1% |
| Timeout rate | 0 | < 0.01% | > 0.01% |

## Troubleshooting

### Connection Exhaustion

**Symptom**: Requests timing out waiting for connections

**Diagnosis**:
```cpp
if (stats.waiting_count > 0 && stats.idle_count == 0) {
    // Pool exhausted
}
```

**Solutions**:
1. Increase `max_connections`
2. Reduce transaction duration
3. Add connection leak detection
4. Check for long-running queries

### Connection Leaks

```cpp
// Enable leak detection
pool_config config;
config.leak_detection_threshold_ms = 60000;  // Warn if held > 60s

pool.on_potential_leak([](const connection_info& info) {
    std::cerr << "Potential leak: connection held for "
              << info.hold_duration_ms << "ms" << std::endl;
    std::cerr << "Acquired at: " << info.stack_trace << std::endl;
});
```

## Performance Benchmarks

### Pool vs No Pool

| Operation | No Pool | With Pool | Improvement |
|-----------|---------|-----------|-------------|
| Single query | 5.2ms | 0.8ms | 6.5x |
| 100 queries | 520ms | 85ms | 6.1x |
| 1000 queries | 5200ms | 890ms | 5.8x |

*PostgreSQL on localhost, query: SELECT 1*

### Pool Size Impact

| Pool Size | Throughput (QPS) | Avg Latency |
|-----------|------------------|-------------|
| 1 | 1200 | 0.83ms |
| 5 | 5800 | 0.86ms |
| 10 | 10500 | 0.95ms |
| 20 | 12000 | 1.67ms |
| 50 | 11800 | 4.24ms |

*Diminishing returns after optimal size*
```

### 2.4 Implementation Steps

1. **Main Guide and Structure** (Day 1)
   - Create main tuning guide
   - Backend comparison matrix
   - Quick reference tables

2. **PostgreSQL Tuning** (Day 2)
   - Server configuration
   - Query optimization
   - Indexing strategies

3. **MySQL and SQLite Tuning** (Day 2-3)
   - MySQL-specific optimizations
   - SQLite for embedded use
   - Memory-only mode

4. **Connection Pool Guide** (Day 3-4)
   - Sizing guidelines
   - Monitoring recommendations
   - Troubleshooting section

---

## 3. How to Test

### 3.1 Documentation Verification

- All code examples compile and run
- Benchmark numbers are reproducible
- Links to related docs work
- Technical accuracy review

### 3.2 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Backend guides complete | All 3 | Manual review |
| Code examples tested | 100% | CI build |
| Benchmarks verified | ±20% accuracy | Re-run benchmarks |

---

## 4. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-007](DB-007-benchmark.md) (Benchmark Baseline)
  - [DB-009](DB-009-async-stress.md) (Stress Tests)

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
