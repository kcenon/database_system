# DB-007: Establish Performance Benchmark Baseline

**Category**: CI
**Priority**: MEDIUM
**Status**: TODO
**Est. Duration**: 3-5 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Performance benchmarks directory exists (`benchmarks/`) but lacks comprehensive tests
- No baseline metrics established for regression detection
- No automated performance testing in CI
- Manual performance testing only

### Target State
- Comprehensive benchmark suite covering all major operations
- Baseline metrics established and tracked
- Automated performance regression detection in CI
- Historical performance trend visualization
- Alert system for significant regressions

### Scope
**Benchmark Areas**:
- Connection pool operations
- Query builder performance
- CRUD operation latency
- Concurrent operation throughput
- Memory usage patterns

**Infrastructure**:
- Google Benchmark integration
- CI performance job
- Results storage and comparison
- Regression alerting

---

## 2. How to Change

### 2.1 Benchmark Suite Structure

```cpp
// benchmarks/database_benchmarks.cpp
#include <benchmark/benchmark.h>
#include "database/backends/sqlite/sqlite_manager.h"
#include "database/query_builder.h"
#include "database/connection_pool.h"

//=============================================================================
// Connection Pool Benchmarks
//=============================================================================

class ConnectionPoolFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        pool_config config;
        config.min_connections = 5;
        config.max_connections = state.range(0);
        config.connection_string = ":memory:";
        pool_ = std::make_unique<ConnectionPool>(config);
        pool_->initialize();
    }

    void TearDown(const ::benchmark::State&) override {
        pool_.reset();
    }

protected:
    std::unique_ptr<ConnectionPool> pool_;
};

BENCHMARK_DEFINE_F(ConnectionPoolFixture, AcquireRelease)(benchmark::State& state) {
    for (auto _ : state) {
        auto conn = pool_->acquire();
        benchmark::DoNotOptimize(conn);
        pool_->release(conn);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(ConnectionPoolFixture, AcquireRelease)
    ->Arg(10)->Arg(50)->Arg(100)
    ->Unit(benchmark::kMicrosecond);

//=============================================================================
// Query Builder Benchmarks
//=============================================================================

static void BM_QueryBuilder_SimpleSelect(benchmark::State& state) {
    for (auto _ : state) {
        database::sql_query_builder builder;
        auto query = builder
            .select({"id", "name", "email"})
            .from("users")
            .where("active", "=", true)
            .limit(100)
            .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SimpleSelect)->Unit(benchmark::kNanosecond);

static void BM_QueryBuilder_ComplexJoin(benchmark::State& state) {
    for (auto _ : state) {
        database::sql_query_builder builder;
        auto query = builder
            .select({"u.id", "u.name", "o.total", "p.name"})
            .from("users u")
            .join("orders o", "u.id = o.user_id")
            .left_join("products p", "o.product_id = p.id")
            .where("o.status", "=", std::string("completed"))
            .group_by({"u.id", "u.name"})
            .having("SUM(o.total) > 1000")
            .order_by("u.name")
            .limit(50)
            .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_ComplexJoin)->Unit(benchmark::kNanosecond);

//=============================================================================
// CRUD Operation Benchmarks
//=============================================================================

class SQLiteFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override {
        db_ = std::make_unique<database::sqlite_manager>();
        db_->connect(":memory:");
        db_->execute_query(
            "CREATE TABLE benchmark_table ("
            "id INTEGER PRIMARY KEY, "
            "name TEXT, "
            "value REAL, "
            "created_at TEXT)"
        );
    }

    void TearDown(const ::benchmark::State&) override {
        db_.reset();
    }

protected:
    std::unique_ptr<database::sqlite_manager> db_;
};

BENCHMARK_DEFINE_F(SQLiteFixture, SingleInsert)(benchmark::State& state) {
    int id = 0;
    for (auto _ : state) {
        std::string query = "INSERT INTO benchmark_table (id, name, value) VALUES (" +
                           std::to_string(id++) + ", 'test', 123.45)";
        db_->insert_query(query);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SQLiteFixture, SingleInsert)->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(SQLiteFixture, SelectByPK)(benchmark::State& state) {
    // Insert test data
    for (int i = 0; i < 1000; ++i) {
        db_->insert_query("INSERT INTO benchmark_table VALUES (" +
                         std::to_string(i) + ", 'test', 123.45, '2025-01-01')");
    }

    int id = 0;
    for (auto _ : state) {
        auto result = db_->select_query(
            "SELECT * FROM benchmark_table WHERE id = " + std::to_string(id++ % 1000)
        );
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SQLiteFixture, SelectByPK)->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(SQLiteFixture, SelectRange)(benchmark::State& state) {
    // Insert test data
    for (int i = 0; i < 10000; ++i) {
        db_->insert_query("INSERT INTO benchmark_table VALUES (" +
                         std::to_string(i) + ", 'test', " +
                         std::to_string(i * 1.5) + ", '2025-01-01')");
    }

    int range_size = state.range(0);
    for (auto _ : state) {
        auto result = db_->select_query(
            "SELECT * FROM benchmark_table WHERE id < " + std::to_string(range_size)
        );
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SQLiteFixture, SelectRange)
    ->Arg(10)->Arg(100)->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

//=============================================================================
// Concurrent Operation Benchmarks
//=============================================================================

static void BM_ConcurrentInserts(benchmark::State& state) {
    if (state.thread_index() == 0) {
        // Setup in first thread only
    }

    auto db = std::make_unique<database::sqlite_manager>();
    db->connect(":memory:");
    db->execute_query("CREATE TABLE IF NOT EXISTS concurrent_test (id INT, data TEXT)");

    for (auto _ : state) {
        db->insert_query("INSERT INTO concurrent_test VALUES (1, 'data')");
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ConcurrentInserts)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8)
    ->Unit(benchmark::kMicrosecond);

//=============================================================================
// Memory Benchmarks
//=============================================================================

static void BM_LargeResultSet(benchmark::State& state) {
    auto db = std::make_unique<database::sqlite_manager>();
    db->connect(":memory:");
    db->execute_query("CREATE TABLE large_data (id INT, data TEXT)");

    // Insert large dataset
    std::string large_value(1000, 'X');
    for (int i = 0; i < 10000; ++i) {
        db->insert_query("INSERT INTO large_data VALUES (" +
                        std::to_string(i) + ", '" + large_value + "')");
    }

    for (auto _ : state) {
        auto result = db->select_query("SELECT * FROM large_data");
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * 10000 * 1000);
}
BENCHMARK(BM_LargeResultSet)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
```

### 2.2 CMake Benchmark Configuration

```cmake
# benchmarks/CMakeLists.txt

# Find Google Benchmark
find_package(benchmark REQUIRED)

# Benchmark executable
add_executable(database_benchmarks
    database_benchmarks.cpp
)

target_link_libraries(database_benchmarks
    PRIVATE
        database_system
        benchmark::benchmark
        benchmark::benchmark_main
)

# Performance test target
add_custom_target(run_benchmarks
    COMMAND database_benchmarks
        --benchmark_format=json
        --benchmark_out=benchmark_results.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running performance benchmarks..."
)

# Benchmark comparison target
add_custom_target(compare_benchmarks
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/compare_benchmarks.py
        ${CMAKE_BINARY_DIR}/benchmark_baseline.json
        ${CMAKE_BINARY_DIR}/benchmark_results.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Comparing benchmark results..."
)
```

### 2.3 GitHub Actions Performance Job

```yaml
# .github/workflows/benchmarks.yml
name: Performance Benchmarks

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

permissions:
  contents: write
  pull-requests: write

jobs:
  benchmark:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout code
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libbenchmark-dev

      - name: Configure CMake
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_BENCHMARKS=ON

      - name: Build
        run: cmake --build build --target database_benchmarks -j$(nproc)

      - name: Run benchmarks
        run: |
          cd build
          ./benchmarks/database_benchmarks \
            --benchmark_format=json \
            --benchmark_out=benchmark_results.json

      - name: Download baseline
        uses: actions/cache@v3
        with:
          path: build/benchmark_baseline.json
          key: benchmark-baseline-${{ github.base_ref || 'main' }}
          restore-keys: |
            benchmark-baseline-main

      - name: Compare with baseline
        id: comparison
        run: |
          cd build
          python3 ../scripts/compare_benchmarks.py \
            benchmark_baseline.json \
            benchmark_results.json \
            --threshold 10 \
            --output comparison.md

      - name: Comment on PR
        if: github.event_name == 'pull_request'
        uses: marocchino/sticky-pull-request-comment@v2
        with:
          header: benchmark
          path: build/comparison.md

      - name: Update baseline
        if: github.ref == 'refs/heads/main'
        run: |
          cp build/benchmark_results.json build/benchmark_baseline.json

      - name: Save baseline
        if: github.ref == 'refs/heads/main'
        uses: actions/cache@v3
        with:
          path: build/benchmark_baseline.json
          key: benchmark-baseline-main

      - name: Fail on regression
        if: steps.comparison.outputs.regression == 'true'
        run: |
          echo "Performance regression detected!"
          exit 1
```

### 2.4 Benchmark Comparison Script

```python
#!/usr/bin/env python3
# scripts/compare_benchmarks.py

import json
import sys
import argparse
from pathlib import Path

def load_benchmarks(filepath):
    with open(filepath) as f:
        data = json.load(f)
    return {b['name']: b for b in data['benchmarks']}

def compare_benchmarks(baseline, current, threshold_percent):
    regressions = []
    improvements = []
    results = []

    for name, current_bench in current.items():
        if name not in baseline:
            continue

        baseline_bench = baseline[name]
        baseline_time = baseline_bench['real_time']
        current_time = current_bench['real_time']

        if baseline_time == 0:
            continue

        change_percent = ((current_time - baseline_time) / baseline_time) * 100

        result = {
            'name': name,
            'baseline': baseline_time,
            'current': current_time,
            'change_percent': change_percent,
            'unit': current_bench.get('time_unit', 'ns')
        }
        results.append(result)

        if change_percent > threshold_percent:
            regressions.append(result)
        elif change_percent < -threshold_percent:
            improvements.append(result)

    return results, regressions, improvements

def generate_markdown(results, regressions, improvements, threshold):
    lines = ["## Performance Benchmark Results\n"]

    if regressions:
        lines.append("### ⚠️ Regressions Detected\n")
        lines.append("| Benchmark | Baseline | Current | Change |")
        lines.append("|-----------|----------|---------|--------|")
        for r in regressions:
            lines.append(f"| {r['name']} | {r['baseline']:.2f} {r['unit']} | "
                        f"{r['current']:.2f} {r['unit']} | "
                        f"+{r['change_percent']:.1f}% ⚠️ |")
        lines.append("")

    if improvements:
        lines.append("### ✅ Improvements\n")
        lines.append("| Benchmark | Baseline | Current | Change |")
        lines.append("|-----------|----------|---------|--------|")
        for r in improvements:
            lines.append(f"| {r['name']} | {r['baseline']:.2f} {r['unit']} | "
                        f"{r['current']:.2f} {r['unit']} | "
                        f"{r['change_percent']:.1f}% ✅ |")
        lines.append("")

    lines.append("### All Results\n")
    lines.append("<details><summary>Click to expand</summary>\n")
    lines.append("| Benchmark | Baseline | Current | Change |")
    lines.append("|-----------|----------|---------|--------|")
    for r in sorted(results, key=lambda x: x['name']):
        indicator = ""
        if r['change_percent'] > threshold:
            indicator = "⚠️"
        elif r['change_percent'] < -threshold:
            indicator = "✅"
        lines.append(f"| {r['name']} | {r['baseline']:.2f} {r['unit']} | "
                    f"{r['current']:.2f} {r['unit']} | "
                    f"{r['change_percent']:+.1f}% {indicator} |")
    lines.append("</details>")

    return '\n'.join(lines)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline', help='Baseline benchmark JSON file')
    parser.add_argument('current', help='Current benchmark JSON file')
    parser.add_argument('--threshold', type=float, default=10,
                       help='Regression threshold percentage')
    parser.add_argument('--output', help='Output markdown file')
    args = parser.parse_args()

    if not Path(args.baseline).exists():
        print("No baseline found, creating initial baseline")
        Path(args.baseline).write_text(Path(args.current).read_text())
        sys.exit(0)

    baseline = load_benchmarks(args.baseline)
    current = load_benchmarks(args.current)

    results, regressions, improvements = compare_benchmarks(
        baseline, current, args.threshold
    )

    markdown = generate_markdown(results, regressions, improvements, args.threshold)

    if args.output:
        Path(args.output).write_text(markdown)
    else:
        print(markdown)

    # Set output for GitHub Actions
    if regressions:
        print("::set-output name=regression::true")
        sys.exit(1)
    else:
        print("::set-output name=regression::false")

if __name__ == '__main__':
    main()
```

### 2.5 Implementation Steps

1. **Benchmark Suite Setup** (Day 1)
   - Install Google Benchmark
   - Create benchmark structure
   - Write initial benchmarks

2. **Baseline Establishment** (Day 2)
   - Run benchmarks on main branch
   - Store baseline results
   - Document expected performance

3. **CI Integration** (Day 3)
   - Create GitHub Actions workflow
   - Set up baseline caching
   - Configure regression detection

4. **Comparison & Alerting** (Day 4)
   - Write comparison script
   - Add PR comments
   - Configure alerts

5. **Documentation** (Day 5)
   - Update BASELINE.md
   - Document benchmark usage
   - Add performance tuning guide reference

---

## 3. How to Test

### 3.1 Local Benchmark Execution

```bash
# Build benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build --target database_benchmarks

# Run benchmarks
./build/benchmarks/database_benchmarks

# Run with JSON output
./build/benchmarks/database_benchmarks \
    --benchmark_format=json \
    --benchmark_out=results.json

# Run specific benchmark
./build/benchmarks/database_benchmarks \
    --benchmark_filter="BM_QueryBuilder*"
```

### 3.2 Comparison Testing

```bash
# Save baseline
./build/benchmarks/database_benchmarks \
    --benchmark_out=baseline.json

# Make changes...

# Run new benchmarks
./build/benchmarks/database_benchmarks \
    --benchmark_out=current.json

# Compare
python3 scripts/compare_benchmarks.py baseline.json current.json
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Benchmark count | 15+ benchmarks | Code review |
| Baseline documented | Yes | BASELINE.md |
| CI integration | Runs on every PR | GitHub Actions |
| Regression detection | 10% threshold | Comparison script |
| PR comments | Performance summary | PR checks |

### 3.4 Initial Baseline Targets

| Benchmark | Target | Acceptable Range |
|-----------|--------|------------------|
| Pool acquire/release | <5 μs | ±20% |
| Simple SELECT build | <500 ns | ±20% |
| Complex JOIN build | <2000 ns | ±20% |
| Single INSERT (SQLite) | <50 μs | ±30% |
| SELECT by PK (SQLite) | <20 μs | ±30% |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Flaky benchmark results | HIGH | Multiple runs, statistical analysis |
| CI machine variability | MEDIUM | Relative comparisons only |
| Threshold too strict | MEDIUM | Start with 10%, adjust as needed |
| Missing important metrics | LOW | Iteratively add benchmarks |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-009](DB-009-async-stress.md) (Stress Tests)
  - [DB-013](DB-013-tuning-guide.md) (Performance Tuning Guide)

---

## 6. Notes

- Benchmarks should run in Release mode for accurate results
- Avoid I/O in hot paths of benchmarks
- Consider adding memory benchmarks using custom allocators
- Google Benchmark provides built-in statistics (mean, stddev)

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
