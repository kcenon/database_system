# DB-006: Set Test Coverage Threshold (80%)

**Category**: CI
**Priority**: MEDIUM
**Status**: TODO
**Est. Duration**: 2-3 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Test coverage is approximately 65% (as noted in CURRENT_STATE_KO.md)
- No automated coverage threshold enforcement in CI/CD
- No coverage trend tracking over time
- Coverage reports generated locally but not integrated into CI

### Target State
- Automated coverage measurement in CI pipeline
- 80% minimum line coverage threshold enforced
- Coverage reports published to PR comments
- Coverage trend tracking with historical data
- Coverage badges in repository README

### Scope
**CI Configuration Files**:
- `.github/workflows/coverage.yml` (new)
- `CMakeLists.txt` (coverage build options)
- `codecov.yml` or `coveralls.yml` (coverage service config)

**Coverage Targets**:
- Line coverage: 80% minimum
- Branch coverage: 70% minimum (recommended)
- Function coverage: 85% minimum (recommended)

---

## 2. How to Change

### 2.1 CMake Coverage Configuration

```cmake
# CMakeLists.txt additions

# Coverage build option
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

        # Find coverage tools
        find_program(GCOV_PATH gcov)
        find_program(LCOV_PATH lcov)
        find_program(GCOVR_PATH gcovr)

        if(NOT GCOV_PATH)
            message(FATAL_ERROR "gcov not found! Aborting...")
        endif()
    else()
        message(WARNING "Coverage only supported with GCC/Clang")
    endif()
endif()

# Coverage target
if(ENABLE_COVERAGE)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --exclude '${CMAKE_SOURCE_DIR}/tests/.*'
            --exclude '${CMAKE_SOURCE_DIR}/third_party/.*'
            --exclude '${CMAKE_SOURCE_DIR}/build/.*'
            --html --html-details
            --output ${CMAKE_BINARY_DIR}/coverage/index.html
            --xml ${CMAKE_BINARY_DIR}/coverage/coverage.xml
            --print-summary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report..."
    )

    add_custom_target(coverage-check
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --exclude '${CMAKE_SOURCE_DIR}/tests/.*'
            --exclude '${CMAKE_SOURCE_DIR}/third_party/.*'
            --fail-under-line 80
            --fail-under-branch 70
            --print-summary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Checking coverage thresholds..."
    )
endif()
```

### 2.2 GitHub Actions Workflow

```yaml
# .github/workflows/coverage.yml
name: Code Coverage

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout code
        uses: actions/checkout@v4
        with:
          fetch-depth: 0  # For coverage diff

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y gcovr lcov
          pip install gcovr

      - name: Configure CMake with coverage
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_COVERAGE=ON \
            -DBUILD_TESTING=ON

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure -j$(nproc)

      - name: Generate coverage report
        run: |
          cd build
          cmake --build . --target coverage

      - name: Check coverage threshold
        run: |
          cd build
          cmake --build . --target coverage-check

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v3
        with:
          files: build/coverage/coverage.xml
          fail_ci_if_error: true
          verbose: true

      - name: Upload coverage artifacts
        uses: actions/upload-artifact@v3
        with:
          name: coverage-report
          path: build/coverage/

      - name: Comment coverage on PR
        if: github.event_name == 'pull_request'
        uses: marocchino/sticky-pull-request-comment@v2
        with:
          header: coverage
          message: |
            ## Code Coverage Report

            | Metric | Coverage |
            |--------|----------|
            | Lines | ${{ steps.coverage.outputs.line_coverage }}% |
            | Branches | ${{ steps.coverage.outputs.branch_coverage }}% |

            [Full Report](https://codecov.io/gh/${{ github.repository }}/pull/${{ github.event.pull_request.number }})
```

### 2.3 Codecov Configuration

```yaml
# codecov.yml
coverage:
  precision: 2
  round: down
  range: "70...100"

  status:
    project:
      default:
        target: 80%
        threshold: 2%
        if_ci_failed: error

    patch:
      default:
        target: 80%
        threshold: 5%

parsers:
  gcov:
    branch_detection:
      conditional: yes
      loop: yes
      method: no
      macro: no

comment:
  layout: "reach, diff, flags, files"
  behavior: default
  require_changes: true

flags:
  unit:
    paths:
      - database/
    carryforward: true

ignore:
  - "tests/**/*"
  - "third_party/**/*"
  - "samples/**/*"
  - "benchmarks/**/*"
```

### 2.4 Coverage Badge Integration

```markdown
<!-- README.md -->
# Database System

[![codecov](https://codecov.io/gh/owner/database_system/branch/main/graph/badge.svg)](https://codecov.io/gh/owner/database_system)
[![Coverage Status](https://coveralls.io/repos/github/owner/database_system/badge.svg?branch=main)](https://coveralls.io/github/owner/database_system?branch=main)

...
```

### 2.5 Local Coverage Script

```bash
#!/bin/bash
# scripts/coverage.sh

set -e

BUILD_DIR=${BUILD_DIR:-build}
COVERAGE_DIR=${BUILD_DIR}/coverage

echo "=== Building with coverage enabled ==="
cmake -B ${BUILD_DIR} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON \
    -DBUILD_TESTING=ON

cmake --build ${BUILD_DIR} -j$(nproc)

echo "=== Running tests ==="
cd ${BUILD_DIR}
ctest --output-on-failure -j$(nproc)

echo "=== Generating coverage report ==="
cmake --build . --target coverage

echo "=== Coverage Summary ==="
gcovr --root .. \
    --exclude '../tests/.*' \
    --exclude '../third_party/.*' \
    --print-summary

echo ""
echo "HTML report: ${COVERAGE_DIR}/index.html"

# Open report in browser (macOS)
if [[ "$OSTYPE" == "darwin"* ]]; then
    open ${COVERAGE_DIR}/index.html
fi
```

### 2.6 Implementation Steps

1. **CMake Configuration** (Day 1)
   - Add ENABLE_COVERAGE option
   - Configure compiler flags for coverage
   - Create coverage and coverage-check targets

2. **GitHub Actions Setup** (Day 1-2)
   - Create coverage workflow
   - Integrate with Codecov
   - Add PR comments

3. **Threshold Enforcement** (Day 2)
   - Configure codecov.yml
   - Set up branch protection rules
   - Test threshold failures

4. **Documentation & Badges** (Day 3)
   - Add coverage badges to README
   - Document local coverage generation
   - Update CONTRIBUTING.md

---

## 3. How to Test

### 3.1 Local Verification

```bash
# Generate coverage locally
./scripts/coverage.sh

# Check specific threshold
cd build
gcovr --root .. \
    --exclude '../tests/.*' \
    --fail-under-line 80

# Generate detailed HTML report
gcovr --root .. \
    --html-details coverage/index.html
```

### 3.2 CI Verification

1. Create a PR with test changes
2. Verify coverage job runs
3. Check Codecov integration
4. Verify threshold enforcement
5. Confirm PR comment generated

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| CI coverage job | Runs on every PR | GitHub Actions |
| Line coverage threshold | 80% enforced | CI failure on <80% |
| Coverage report | Generated and published | Codecov dashboard |
| PR comments | Coverage summary posted | PR checks |
| README badge | Shows current coverage | Visual check |

### 3.4 Threshold Testing

```bash
# Temporarily lower threshold to verify enforcement
# In codecov.yml, set target: 95%
# Push changes and verify CI fails
```

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Coverage drop blocks PRs | HIGH | Set reasonable threshold (80%) |
| Flaky coverage numbers | MEDIUM | Use threshold buffer (2%) |
| CI time increase | LOW | Parallel test execution |
| External service dependency | LOW | Keep local coverage scripts |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-001](DB-001-backend-tests.md) (Backend Tests)
  - [DB-002](DB-002-orm-tests.md) (ORM Tests)
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)

---

## 6. Notes

- Start with 80% threshold, increase gradually as tests are added
- Exclude test files, third-party code, and generated code
- Branch coverage is harder to achieve - 70% is a reasonable target
- Consider using LCOV for more detailed reports if needed

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
