# Database System Performance Tuning Guide

## Overview

This guide provides comprehensive performance tuning recommendations
for the Database System across different backends and use cases.

## Backend Selection Guide

### Workload Characteristics Matrix

| Workload Type | Recommended Backend | Rationale |
|---------------|---------------------|-----------|
| Read-heavy OLTP | PostgreSQL | Superior indexing, parallel queries |
| Write-heavy OLTP | MySQL (InnoDB) | Efficient transaction batching |
| Embedded/Local | SQLite | Zero configuration, in-process |
| Complex Analytics | PostgreSQL | Advanced query planner |
| Simple Key-Value | SQLite | Minimal overhead |

### Backend Performance Comparison

```
Benchmark: 10,000 mixed operations (70% read, 30% write)

PostgreSQL:  850 ops/sec  (best for complex queries)
MySQL:       920 ops/sec  (best for simple writes)
SQLite:     1200 ops/sec  (best for embedded use)
```

## Connection Pool Tuning

### Pool Size Formula

```
Optimal Pool Size = (Core Count * 2) + Effective Spindle Count
```

For SSD-based systems:
```
Optimal Pool Size = Core Count * 2 + 1
```

### Configuration Example

```cpp
#include <database/core/database_backend.h>

core::connection_pool_config pool_config;
pool_config.min_connections = 5;     // Minimum warm connections
pool_config.max_connections = 20;    // Maximum connections
pool_config.connection_timeout = std::chrono::seconds(30);
pool_config.idle_timeout = std::chrono::minutes(5);
pool_config.validation_interval = std::chrono::seconds(30);
```

### Pool Size Recommendations

| Use Case | Min Connections | Max Connections |
|----------|-----------------|-----------------|
| Development | 1 | 5 |
| Small Application | 2 | 10 |
| Medium Application | 5 | 20 |
| High Traffic | 10 | 50 |

## Query Optimization

### General Principles

1. **Use Prepared Statements**
```cpp
// Good: Parameterized query
auto stmt = db->prepare("SELECT * FROM users WHERE id = ?");
stmt->bind(1, user_id);
auto result = stmt->execute();

// Avoid: String concatenation
auto result = db->select_query("SELECT * FROM users WHERE id = " + user_id);
```

2. **Limit Result Sets**
```cpp
// Good: Limit results
auto result = db->select_query("SELECT * FROM logs LIMIT 100");

// Avoid: Unbounded queries
auto result = db->select_query("SELECT * FROM logs");
```

3. **Select Only Required Columns**
```cpp
// Good: Specific columns
auto result = db->select_query("SELECT id, name FROM users");

// Avoid: SELECT *
auto result = db->select_query("SELECT * FROM users");
```

## Backend-Specific Tuning

### PostgreSQL

**Key Settings:**
```sql
-- Connection settings
max_connections = 100
shared_buffers = 256MB        -- 25% of RAM
effective_cache_size = 768MB  -- 75% of RAM
work_mem = 4MB

-- Write performance
wal_buffers = 16MB
checkpoint_completion_target = 0.9
```

**Index Strategy:**
- Use B-tree for equality and range queries
- Use GIN for full-text search
- Use BRIN for large, naturally ordered tables

### MySQL

**Key Settings:**
```ini
# InnoDB settings
innodb_buffer_pool_size = 1G  # 70-80% of RAM
innodb_log_file_size = 256M
innodb_flush_log_at_trx_commit = 2  # Performance vs durability

# Query cache (MySQL 5.7)
query_cache_type = 1
query_cache_size = 64M
```

**Index Strategy:**
- Use covering indexes for read-heavy queries
- Consider composite indexes for multi-column WHERE
- Use EXPLAIN to verify index usage

### SQLite

**Key Settings:**
```cpp
db->execute_query("PRAGMA journal_mode = WAL");
db->execute_query("PRAGMA synchronous = NORMAL");
db->execute_query("PRAGMA cache_size = -64000");  // 64MB
db->execute_query("PRAGMA temp_store = MEMORY");
```

**Best Practices:**
- Use WAL mode for concurrent read/write
- Batch writes in transactions
- Vacuum periodically for large databases

## Caching Strategy

### Query Result Caching

```cpp
cache_config cache;
cache.enabled = true;
cache.max_entries = 10000;
cache.default_ttl = std::chrono::seconds(300);

// Cache invalidation patterns
cache.invalidate_on_write = true;
cache.table_tracking = true;

gateway.configure_caching(cache);
```

### Cache Size Guidelines

| Dataset Size | Recommended Cache Size |
|--------------|------------------------|
| < 100MB | Cache entire result set |
| 100MB - 1GB | 10-20% of dataset |
| > 1GB | Monitor hit rate, adjust |

## Monitoring and Diagnostics

### Key Metrics to Monitor

1. **Connection Pool**
   - Active connections
   - Wait time for connections
   - Connection creation rate

2. **Query Performance**
   - Average query latency
   - Slow query count
   - Query cache hit rate

3. **Resource Utilization**
   - Memory usage
   - CPU utilization
   - Disk I/O

### Example Monitoring Code

```cpp
auto stats = gateway.get_stats();

std::cout << "Total queries: " << stats.total_queries << std::endl;
std::cout << "Avg latency: " << stats.avg_latency_ms << "ms" << std::endl;
std::cout << "Cache hit rate: "
          << (stats.cache_hits * 100.0 / (stats.cache_hits + stats.cache_misses))
          << "%" << std::endl;
```

## Common Performance Issues

### Issue: High Latency

**Symptoms:** Slow query response times

**Solutions:**
1. Check indexes with EXPLAIN
2. Increase connection pool size
3. Enable query caching
4. Optimize query structure

### Issue: Connection Exhaustion

**Symptoms:** "Too many connections" errors

**Solutions:**
1. Increase max_connections
2. Implement connection pooling
3. Set proper connection timeouts
4. Close idle connections

### Issue: Memory Pressure

**Symptoms:** OOM errors, swapping

**Solutions:**
1. Reduce cache size
2. Limit result set sizes
3. Use streaming for large results
4. Optimize queries to reduce memory

## Performance Checklist

- [ ] Connection pool properly sized
- [ ] Prepared statements used for repeated queries
- [ ] Appropriate indexes created
- [ ] Query cache configured
- [ ] Slow query logging enabled
- [ ] Monitoring in place
- [ ] Backend-specific tuning applied

## See Also

- [POSTGRESQL_TUNING.md](POSTGRESQL_TUNING.md) - PostgreSQL specific tuning
- [MYSQL_TUNING.md](MYSQL_TUNING.md) - MySQL specific tuning
- [SQLITE_TUNING.md](SQLITE_TUNING.md) - SQLite specific tuning
- [BENCHMARKS.md](BENCHMARKS.md) - Performance benchmark results
