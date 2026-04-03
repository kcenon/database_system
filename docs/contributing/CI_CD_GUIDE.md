---
doc_id: "DBS-GUID-009"
doc_title: "CI/CD Guide for database_system"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# CI/CD Guide for database_system

> **SSOT**: This document is the single source of truth for **CI/CD Guide for database_system**.

**Version:** 0.1.0
**Last Updated:** 2025-11-11
**Status:** Stable
**Audience:** Contributors, Maintainers

---

## Overview

This guide explains the Continuous Integration and Continuous Deployment (CI/CD) pipelines for database_system. Understanding these workflows will help you:

- Run checks locally before pushing
- Interpret CI failures
- Add new tests to the pipeline
- Debug platform-specific issues

---

## Table of Contents

- [CI/CD Pipeline Overview](#cicd-pipeline-overview)
- [GitHub Actions Workflows](#github-actions-workflows)
- [Running Checks Locally](#running-checks-locally)
- [Understanding CI Failures](#understanding-ci-failures)
- [Adding New Tests](#adding-new-tests)
- [Performance Benchmarks](#performance-benchmarks)
- [Troubleshooting](#troubleshooting)

---

## CI/CD Pipeline Overview

### Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Push to branch / Open Pull Request                          │
└──────────────────┬──────────────────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌────────┐  ┌────────────┐  ┌──────────┐
│  Main  │  │ Sanitizers │  │ Coverage │
│   CI   │  │  (TSan,    │  │          │
│        │  │  ASan,     │  │          │
│        │  │  UBSan)    │  │          │
└────┬───┘  └──────┬─────┘  └────┬─────┘
     │             │             │
     ▼             ▼             ▼
┌─────────────────────────────────────┐
│  Static Analysis                    │
│  - Clang-Tidy                       │
│  - Cppcheck                         │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  Security Scan                       │
│  - Dependency vulnerabilities        │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  Integration Tests                   │
│  - Backend-specific tests            │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  Benchmarks                          │
│  - Performance regression tests      │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  Documentation Build (Doxygen)       │
└─────────────────────────────────────┘
```

### Workflow Triggers

| Workflow | Trigger | Runs On |
|----------|---------|---------|
| Main CI | Push to main, phase-* branches; All PRs | Every commit |
| Sanitizers | Push to main, phase-* branches; All PRs | Every commit |
| Static Analysis | Push to main; PRs to main | Every commit |
| Security Scan | Push to main; Weekly schedule | Push + Weekly |
| Integration Tests | PRs to main (on-demand) | Manual/PR label |
| Benchmarks | Push to main; Monthly schedule | Push + Monthly |
| Coverage | PRs to main | PRs only |
| Doxygen | Push to main | On main branch |

---

## GitHub Actions Workflows

### 1. Main CI Pipeline (`ci.yml`)

**Purpose**: Build and test across multiple platforms and compilers

**Matrix**:
- **Platforms**: Ubuntu 22.04, macOS 13, Windows 2022
- **Compilers**: GCC, Clang, MSVC
- **Build Type**: Debug
- **Configuration**: Standalone (no external dependencies)

**Steps**:
1. Checkout code with submodules
2. Install dependencies (CMake, Ninja, GoogleTest)
3. Configure CMake with backend flags
4. Build with parallel compilation
5. Run unit tests (timeout: 30s per test)
6. Upload logs on failure

**Key Configuration**:
```yaml
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_WITH_COMMON_SYSTEM=OFF \
  -DUSE_UNIT_TEST=ON \
  -DBUILD_DATABASE_SAMPLES=ON \
  -DDATABASE_BUILD_INTEGRATION_TESTS=OFF \
  -DUSE_POSTGRESQL=OFF
```

**Current Status**: ✅ Passing (22/23 tests)

---

### 2. Sanitizers (`ci.yml`)

**Purpose**: Detect memory issues, data races, and undefined behavior

**Sanitizers Run**:
1. **ThreadSanitizer (TSan)**
   - Detects data races
   - Flags: `-fsanitize=thread -g -O1`
   - Environment: `TSAN_OPTIONS=second_deadlock_stack=1`

2. **AddressSanitizer (ASan)**
   - Detects memory errors (use-after-free, buffer overflows)
   - Flags: `-fsanitize=address -fno-omit-frame-pointer -g -O1`
   - Environment: `ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1`

3. **UndefinedBehaviorSanitizer (UBSan)**
   - Detects undefined behavior
   - Flags: `-fsanitize=undefined -fno-omit-frame-pointer -g -O1`
   - Environment: `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0`

**Status**: ⚠️ Continue-on-error enabled (Phase 0 - data collection)

---

### 3. Static Analysis (`static-analysis.yml`)

**Purpose**: Catch potential bugs and style issues

**Tools**:
- **Clang-Tidy**: Modern C++ linter
- **Cppcheck**: Static code analyzer
- **Include-what-you-use**: Header dependency checker

**Checks**:
- Modernize recommendations
- Performance issues
- Readability improvements
- Bug-prone patterns
- Security vulnerabilities

---

### 4. Dependency Security Scan (`dependency-security-scan.yml`)

**Purpose**: Identify vulnerable dependencies

**Scans**:
- vcpkg dependencies
- System libraries
- Transitive dependencies

**Schedule**: Weekly + on push to main

---

### 5. Integration Tests (`integration-tests.yml`)

**Purpose**: Test with real database backends

**Backends Tested**:
- PostgreSQL 15
- SQLite 3
- MongoDB 6.0
- Redis 7.0

**Setup**: Uses Docker containers for database services

**Trigger**: Manual or on PR with label `run-integration-tests`

---

### 6. Benchmarks (`benchmarks.yml`)

**Purpose**: Track performance over time

**Metrics**:
- Connection pool performance
- Query latency (simple, complex, joins)
- Bulk insert throughput
- Transaction overhead
- Cache hit rates

**Baseline**: See [PERFORMANCE_BENCHMARKS.md](../performance/BENCHMARKS.md)

**Schedule**: On push to main + monthly

---

### 7. Coverage (`coverage.yml`)

**Purpose**: Measure test coverage

**Tool**: lcov + gcov

**Target**: >80% coverage for new code

**Reports**: Uploaded to Codecov (if configured)

---

### 8. Documentation Build (`build-Doxygen.yaml`)

**Purpose**: Generate API documentation

**Output**: HTML documentation from Doxygen comments

**Deployment**: GitHub Pages (if enabled)

---

## Running Checks Locally

### Prerequisites

```bash
# Install tools
sudo apt-get install cmake ninja-build clang clang-tidy cppcheck lcov

# Install compilers
sudo apt-get install g++-11 clang-15

# Install Google Test
sudo apt-get install libgtest-dev libgmock-dev
```

### 1. Main CI Build

Replicate the CI build locally:

```bash
# Configure (same as CI)
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_WITH_COMMON_SYSTEM=OFF \
  -DUSE_UNIT_TEST=ON \
  -DBUILD_DATABASE_SAMPLES=ON \
  -DDATABASE_BUILD_INTEGRATION_TESTS=OFF \
  -DUSE_POSTGRESQL=OFF

# Build
cmake --build build --config Debug --parallel

# Test
cd build
ctest -C Debug --output-on-failure --timeout 30
```

### 2. Sanitizers

**ThreadSanitizer**:
```bash
cmake -B build-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DUSE_UNIT_TEST=ON \
  -DUSE_POSTGRESQL=OFF

cmake --build build-tsan
cd build-tsan
export TSAN_OPTIONS=second_deadlock_stack=1
ctest --output-on-failure
```

**AddressSanitizer**:
```bash
cmake -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  -DUSE_UNIT_TEST=ON \
  -DUSE_POSTGRESQL=OFF

cmake --build build-asan
cd build-asan
export ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1
ctest --output-on-failure
```

**UndefinedBehaviorSanitizer**:
```bash
cmake -B build-ubsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" \
  -DUSE_UNIT_TEST=ON \
  -DUSE_POSTGRESQL=OFF

cmake --build build-ubsan
cd build-ubsan
export UBSAN_OPTIONS=print_stacktrace=1
ctest --output-on-failure
```

### 3. Static Analysis

**Clang-Tidy**:
```bash
# Generate compile_commands.json
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build database/**/*.cpp -- -std=c++17
```

**Cppcheck**:
```bash
cppcheck --enable=all --std=c++17 \
  --suppress=missingIncludeSystem \
  -I database/include \
  database/
```

### 4. Code Coverage

```bash
# Configure with coverage flags
cmake -B build-coverage -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  -DUSE_UNIT_TEST=ON \
  -DUSE_POSTGRESQL=OFF

# Build and test
cmake --build build-coverage
cd build-coverage
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_report

# View report
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### 5. Integration Tests

**Setup databases with Docker**:
```bash
# PostgreSQL
docker run -d --name postgres-test \
  -e POSTGRES_PASSWORD=test \
  -p 5432:5432 postgres:15

# MongoDB
docker run -d --name mongo-test \
  -p 27017:27017 mongo:6.0

# Redis
docker run -d --name redis-test \
  -p 6379:6379 redis:7.0
```

**Build and test**:
```bash
cmake -B build -G Ninja \
  -DDATABASE_BUILD_INTEGRATION_TESTS=ON \
  -DUSE_POSTGRESQL=ON \
  -DUSE_SQLITE=ON \
  -DUSE_MONGODB=ON \
  -DUSE_REDIS=ON

cmake --build build
cd build
ctest -R integration --output-on-failure
```

**Cleanup**:
```bash
docker stop postgres-test mongo-test redis-test
docker rm postgres-test mongo-test redis-test
```

---

## Understanding CI Failures

### Common Failure Patterns

#### 1. Build Failure

**Symptom**: Compilation errors

**Example**:
```
error: 'std::optional' does not name a type
```

**Diagnosis**:
- Check compiler version (C++17 required)
- Verify include paths
- Check for missing dependencies

**Solution**:
```bash
# Check compiler version
g++ --version  # Should be 7.0+
clang++ --version  # Should be 5.0+

# Update compiler if needed
sudo apt-get install g++-11
```

#### 2. Test Failure

**Symptom**: CTest reports test failures

**Example**:
```
Test #5: connection_pool_test ...................***Failed  0.12 sec
```

**Diagnosis**:
1. Check test output: `ctest --output-on-failure --rerun-failed`
2. Run specific test: `./build/tests/connection_pool_test`
3. Use debugger: `gdb ./build/tests/connection_pool_test`

**Common causes**:
- Backend not available
- Port conflicts
- Timeout issues
- Race conditions (check with TSan)

#### 3. Sanitizer Failure

**Symptom**: TSan/ASan/UBSan reports issues

**ThreadSanitizer Example**:
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7b0400000000 by thread T1:
    #0 connection_pool::acquire() connection_pool.cpp:123
```

**Diagnosis**:
- Identify the data race location
- Check for missing synchronization
- Verify lock acquisition order

**Solution**:
- Add mutex protection
- Use atomic operations
- Apply proper lock ordering

**AddressSanitizer Example**:
```
ERROR: AddressSanitizer: heap-use-after-free
```

**Diagnosis**:
- Check object lifetime
- Verify shared_ptr usage
- Look for dangling pointers

#### 4. Static Analysis Warnings

**Clang-Tidy Example**:
```
warning: use nullptr instead of NULL [modernize-use-nullptr]
```

**Solution**:
```cpp
// Before
void* ptr = NULL;

// After
void* ptr = nullptr;
```

#### 5. Coverage Regression

**Symptom**: Coverage drops below threshold

**Solution**:
- Add tests for new code
- Test error paths
- Test edge cases

---

## Adding New Tests

### Unit Test

1. Create test file in `tests/`:
```cpp
// tests/my_feature_test.cpp
#include <gtest/gtest.h>
#include <database/my_feature.h>

TEST(MyFeature, BasicUsage) {
    MyFeature feature;
    EXPECT_TRUE(feature.works());
}

TEST(MyFeature, ErrorHandling) {
    MyFeature feature;
    EXPECT_THROW(feature.invalid_operation(), std::runtime_error);
}
```

2. Add to `tests/CMakeLists.txt`:
```cmake
add_executable(my_feature_test my_feature_test.cpp)
target_link_libraries(my_feature_test database_system GTest::gtest_main)
gtest_discover_tests(my_feature_test)
```

3. Run test:
```bash
cmake --build build
cd build
ctest -R my_feature_test --output-on-failure
```

### Integration Test

1. Create test in `integration_tests/`:
```cpp
// integration_tests/postgres_feature_test.cpp
#include <gtest/gtest.h>
#include <database/unified_database_system.h>

TEST(PostgresIntegration, MyFeature) {
    auto db = create_unified_database("postgresql://localhost/testdb");
    // Test with real database
}
```

2. Add to `integration_tests/CMakeLists.txt`:
```cmake
if(USE_POSTGRESQL)
    add_executable(postgres_feature_test postgres_feature_test.cpp)
    target_link_libraries(postgres_feature_test database_system GTest::gtest_main)
    gtest_discover_tests(postgres_feature_test)
endif()
```

3. Run integration test:
```bash
# Start PostgreSQL (Docker)
docker run -d -p 5432:5432 -e POSTGRES_PASSWORD=test postgres:15

# Run test
cmake --build build
cd build
ctest -R postgres_feature_test --output-on-failure
```

### Benchmark

1. Create benchmark in `benchmarks/`:
```cpp
// benchmarks/my_feature_benchmark.cpp
#include <benchmark/benchmark.h>
#include <database/my_feature.h>

static void BM_MyFeature(benchmark::State& state) {
    MyFeature feature;
    for (auto _ : state) {
        feature.operation();
    }
}
BENCHMARK(BM_MyFeature);

BENCHMARK_MAIN();
```

2. Add to `benchmarks/CMakeLists.txt`:
```cmake
add_executable(my_feature_benchmark my_feature_benchmark.cpp)
target_link_libraries(my_feature_benchmark database_system benchmark::benchmark)
```

3. Run benchmark:
```bash
./build/benchmarks/my_feature_benchmark --benchmark_format=json > results.json
```

---

## Performance Benchmarks

### Running Benchmarks Locally

```bash
# Build benchmarks
cmake -B build -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all benchmarks
cd build/benchmarks
./run_all_benchmarks.sh

# Run specific benchmark
./connection_pool_benchmark
./query_performance_benchmark
```

### Interpreting Results

**Example output**:
```
-----------------------------------------------------------------
Benchmark                          Time           CPU  Iterations
-----------------------------------------------------------------
BM_ConnectionPool/Acquire       0.12 ms      0.12 ms        5698
BM_SimpleQuery/Select           1.20 ms      1.15 ms         608
BM_BulkInsert/1000_rows        45.00 ms     44.50 ms          16
```

**Comparison with baseline**:
- See [BASELINE.md](../performance/BASELINE.md) for expected values
- Regression threshold: >10% slowdown triggers investigation

---

## Troubleshooting

### CI Passes Locally But Fails on GitHub

**Possible causes**:
1. **Platform differences**:
   - Test on multiple OSes locally
   - Use Docker for consistent environment

2. **Timing issues**:
   - Tests may be timing-sensitive
   - GitHub runners may be slower
   - Add retries or longer timeouts

3. **Network dependencies**:
   - CI may have network restrictions
   - Mock external services

### Test Timeout

**Symptom**: Test killed after 30s

**Solution**:
```bash
# Increase timeout in ctest
ctest --timeout 60

# Or fix slow test
# - Reduce test data size
# - Mock expensive operations
# - Split into smaller tests
```

### Sanitizer False Positives

**ThreadSanitizer suppressions**:
```bash
# Create tsan.supp
cat > tsan.supp <<EOF
race:third_party_library
EOF

# Use suppression file
export TSAN_OPTIONS="suppressions=tsan.supp"
```

### Docker Issues in Integration Tests

**Port conflicts**:
```bash
# Check port usage
lsof -i :5432
sudo netstat -tulpn | grep 5432

# Use different port
docker run -p 5433:5432 postgres:15
```

---

## CI/CD Best Practices

### For Contributors

1. **Run tests locally** before pushing
2. **Run sanitizers** on new code
3. **Check static analysis** warnings
4. **Ensure coverage** >80% for new code
5. **Test on multiple platforms** if possible
6. **Write clear commit messages** for CI logs

### For Maintainers

1. **Review CI logs** for all PRs
2. **Investigate flaky tests** immediately
3. **Update baselines** after performance improvements
4. **Monitor sanitizer reports** weekly
5. **Review security scans** weekly
6. **Keep dependencies updated**

---

## Related Documentation

- [Contributing Guide](CONTRIBUTING.md) - General contribution guidelines
- [Build Guide](../guides/BUILD_GUIDE.md) - Detailed build instructions
- [Performance Benchmarks](../performance/BENCHMARKS.md) - Performance baselines
- [Troubleshooting](../guides/TROUBLESHOOTING.md) - General troubleshooting

---

## Contact

For CI/CD issues:
- **GitHub Issues**: [database_system/issues](https://github.com/kcenon/database_system/issues)
- **CI Logs**: Check [GitHub Actions](https://github.com/kcenon/database_system/actions)
- **Maintainers**: See [CONTRIBUTING.md](CONTRIBUTING.md)

---

**Document Maintenance**:
- Review quarterly
- Update after CI changes
- Keep baseline metrics current

**Last Review**: 2025-11-11
**Next Review**: 2026-02-11
