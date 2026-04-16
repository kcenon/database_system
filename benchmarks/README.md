# database_system Performance Benchmarks

Phase 0, Task 0.2: Baseline Performance Benchmarking

## Overview

This directory contains comprehensive performance benchmarks for the database_system, measuring:

- **Query Execution**: Query builder performance and query generation overhead
- **Connection Pool**: Connection acquisition, release, and contention behavior
- **Transactions**: Transaction begin/commit cycles and batch operation performance

## Building

### Prerequisites

```bash
# macOS (Homebrew)
brew install google-benchmark

# Ubuntu/Debian
sudo apt-get install libbenchmark-dev

# From source
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory build
cmake -E chdir build cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build build --config Release
sudo cmake --build build --config Release --target install
```

### Build Benchmarks

```bash
cd database_system
cmake -B build -S . -DDATABASE_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Or use the build target
cd build
make database_benchmarks
```

## Running Benchmarks

### Run All Benchmarks

```bash
./build/benchmarks/database_benchmarks
```

### Run Specific Benchmark Categories

```bash
# Query builder benchmarks only
./build/benchmarks/database_benchmarks --benchmark_filter=QueryBuilder

# Connection pool benchmarks only
./build/benchmarks/database_benchmarks --benchmark_filter=ConnectionPool

# Transaction benchmarks only
./build/benchmarks/database_benchmarks --benchmark_filter=Transaction
```

### Output Formats

```bash
# Console output (default)
./build/benchmarks/database_benchmarks

# JSON output
./build/benchmarks/database_benchmarks --benchmark_format=json

# CSV output
./build/benchmarks/database_benchmarks --benchmark_format=csv

# Save to file
./build/benchmarks/database_benchmarks --benchmark_format=json --benchmark_out=results.json
```

### Advanced Options

```bash
# Run for minimum time (for stable results)
./build/benchmarks/database_benchmarks --benchmark_min_time=5.0

# Specify number of iterations
./build/benchmarks/database_benchmarks --benchmark_repetitions=10

# Show all statistics
./build/benchmarks/database_benchmarks --benchmark_report_aggregates_only=false
```

## Benchmark Categories

### 1. Query Execution Benchmarks

**File**: `query_execution_bench.cpp`

Measures query builder performance:

- Query builder creation overhead
- Simple SELECT query generation
- SELECT with WHERE clauses
- Complex queries (multiple conditions, joins, ORDER BY, LIMIT)
- INSERT, UPDATE, DELETE query generation
- JOIN query generation
- Parameterized queries
- Query complexity scaling (5, 20, 50 columns)

**Target Metrics**:
- Query builder creation: < 100ns
- Simple query generation: < 1μs
- Complex query generation: < 10μs

### 2. Connection Pool Benchmarks

**File**: `connection_pool_bench.cpp`

Measures connection pool performance across three versions:

#### Connection Pool v1 (Baseline)
- Connection pool creation
- Connection acquisition (single-threaded)
- Connection acquisition and release cycle
- Pool size scaling (5, 10, 20, 50 connections)
- Concurrent acquisition (4, 8, 16 threads)
- Pool statistics retrieval
- Health check overhead
- Connection pool contention (4, 8, 16 threads with small pool)

**Target Metrics**:
- Connection acquisition: < 100μs
- Pool creation: < 10ms
- Health check overhead: < 1ms
- Concurrent scaling: near-linear up to pool size

#### Connection Pool v2 (Priority-based with typed_thread_pool)
- Priority-based connection acquisition (HEALTH_CHECK, NORMAL_QUERY, TRANSACTION, CRITICAL)
- Mixed priority concurrent acquisition
- High-load scenario (8, 16, 32 threads)

**Target Metrics**:
- Connection acquisition: < 1μs (from thread_system integration)
- Priority scheduling: Higher priority requests served first
- High-load performance: Better than v1 under contention

#### Connection Pool v3 (Adaptive queue with lock-free optimization)
- Ultra-low latency connection acquisition
- Adaptive queue strategy (switches between mutex and lock-free)
- Throughput measurement (16, 32, 64 threads)
- Priority handling with adaptive queue

**Target Metrics** (from IMPROVEMENT_PLAN.md):
- Connection acquisition latency: < 100ns (65x improvement over v1)
- Throughput: > 1M ops/s
- High-load performance: 4x-7.7x improvement over v2
- Adaptive queue switching: Automatic based on contention metrics

#### Comparison Benchmarks (v1 vs v2 vs v3)
- Single-threaded comparison
- Multi-threaded comparison (16 threads)
- Demonstrates performance progression across versions

### 3. Transaction Benchmarks

**File**: `transaction_bench.cpp`

Measures transaction performance:

- Transaction begin/commit cycle
- Single query within transaction
- Multiple queries (5, 10, 50, 100) within transaction
- Transaction rollback
- Nested transactions (savepoints)
- Mixed read/write operations
- Transaction isolation level overhead
- Batch insert (10, 100, 1000 records)

**Target Metrics**:
- Begin/commit cycle: < 100μs
- Single query transaction: < 1ms
- Batch insert throughput: > 1000 inserts/sec

## Baseline Results

**Documentation**: For comprehensive baseline metrics and performance targets, see [`docs/performance/BASELINE.md`](../docs/performance/BASELINE.md)

Expected baseline ranges (to be confirmed):

| Metric | Target | Acceptable |
|--------|--------|------------|
| Query Builder Create | < 100ns | < 1μs |
| Simple Query Generation | < 1μs | < 10μs |
| Connection Acquisition | < 100μs | < 1ms |
| Transaction Begin/Commit | < 100μs | < 1ms |
| Batch Insert (100 records) | < 100ms | < 500ms |
| Connection Pool Contention | Graceful degradation | < 2x slowdown |

## Interpreting Results

### Understanding Benchmark Output

```
---------------------------------------------------------------
Benchmark                         Time           CPU Iterations
---------------------------------------------------------------
BM_QueryBuilder_Create          156 ns        155 ns    4534891
```

- **Time**: Wall clock time per iteration
- **CPU**: CPU time per iteration
- **Iterations**: Number of times the benchmark was run

### Percentiles

For latency-sensitive benchmarks, focus on p95 and p99:

```bash
# Run with statistics
./build/benchmarks/database_benchmarks --benchmark_enable_random_interleaving=true
```

### Comparison

To compare before/after performance:

```bash
# Baseline
./build/benchmarks/database_benchmarks --benchmark_out=baseline.json --benchmark_out_format=json

# After changes
./build/benchmarks/database_benchmarks --benchmark_out=after.json --benchmark_out_format=json

# Compare (requires benchmark tools)
compare.py baseline.json after.json
```

## CI Integration

Benchmarks are run in CI on every PR to detect performance regressions.

See `.github/workflows/benchmarks.yml` for configuration.

## Troubleshooting

### Google Benchmark Not Found

```bash
# Check installation
find /usr -name "*benchmark*" 2>/dev/null

# Try pkg-config
pkg-config --modversion benchmark

# Reinstall
brew reinstall google-benchmark  # macOS
```

### Build Errors

```bash
# Clean build
rm -rf build
cmake -B build -S . -DDATABASE_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

### Unstable Results

```bash
# Increase minimum time
./build/benchmarks/database_benchmarks --benchmark_min_time=10.0

# Disable CPU frequency scaling (Linux)
sudo cpupower frequency-set --governor performance
```

## Contributing

When adding new benchmarks:

1. Follow existing naming conventions (`BM_Category_SpecificTest`)
2. Use appropriate benchmark types (Fixture, Threaded, etc.)
3. Set meaningful labels and counters
4. Document target metrics in file header
5. Clean up resources in TearDown/after benchmark
6. Update this README with new benchmark description

## References

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [Benchmark Best Practices](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [database_system Architecture](../README.md)

---

**Last Updated**: 2026-04-15
**Phase**: 0 - Foundation and Tooling
**Status**: Baseline measurement in progress
