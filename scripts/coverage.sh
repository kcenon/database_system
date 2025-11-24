#!/bin/bash
# Coverage generation script for database_system
# Usage: ./scripts/coverage.sh [--check] [--open]
#
# Options:
#   --check   Check if coverage meets 80% threshold
#   --open    Open HTML report in browser (macOS/Linux)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-coverage}"
COVERAGE_DIR="${BUILD_DIR}/coverage"
THRESHOLD=80

# Parse arguments
CHECK_THRESHOLD=false
OPEN_REPORT=false
for arg in "$@"; do
    case $arg in
        --check)
            CHECK_THRESHOLD=true
            ;;
        --open)
            OPEN_REPORT=true
            ;;
    esac
done

echo "=== Database System Coverage Report ==="
echo "Project: ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"
echo ""

# Check for gcovr
if ! command -v gcovr &> /dev/null; then
    echo "Error: gcovr is required. Install with: pip install gcovr"
    exit 1
fi

# Configure and build
echo "=== Configuring with coverage enabled ==="
cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON \
    -DUSE_UNIT_TEST=ON \
    -DALLOW_BUILD_WITHOUT_NETWORK_SYSTEM=ON

echo ""
echo "=== Building ==="
cmake --build "${BUILD_DIR}" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Running tests ==="
cd "${BUILD_DIR}"
ctest --output-on-failure -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) || true

echo ""
echo "=== Generating coverage report ==="
mkdir -p "${COVERAGE_DIR}"

gcovr --root "${PROJECT_DIR}" \
    --exclude '.*tests/.*' \
    --exclude '.*third_party/.*' \
    --exclude '.*samples/.*' \
    --exclude '.*benchmarks/.*' \
    --exclude '.*build.*' \
    --html --html-details \
    --output "${COVERAGE_DIR}/index.html" \
    --xml "${COVERAGE_DIR}/coverage.xml" \
    --print-summary

echo ""
echo "HTML report: ${COVERAGE_DIR}/index.html"
echo "XML report:  ${COVERAGE_DIR}/coverage.xml"

# Check threshold if requested
if [ "$CHECK_THRESHOLD" = true ]; then
    echo ""
    echo "=== Checking coverage threshold (${THRESHOLD}%) ==="
    if gcovr --root "${PROJECT_DIR}" \
        --exclude '.*tests/.*' \
        --exclude '.*third_party/.*' \
        --exclude '.*samples/.*' \
        --exclude '.*benchmarks/.*' \
        --exclude '.*build.*' \
        --fail-under-line ${THRESHOLD} \
        --print-summary; then
        echo "Coverage check PASSED"
    else
        echo "Coverage check FAILED: Below ${THRESHOLD}%"
        exit 1
    fi
fi

# Open report if requested
if [ "$OPEN_REPORT" = true ]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        open "${COVERAGE_DIR}/index.html"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        xdg-open "${COVERAGE_DIR}/index.html" 2>/dev/null || echo "Open ${COVERAGE_DIR}/index.html in your browser"
    fi
fi

echo ""
echo "Done!"
