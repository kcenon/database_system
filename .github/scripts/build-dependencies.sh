#!/bin/bash
set -e  # Exit on any error

INSTALL_PREFIX="${1:-$PWD/deps/install}"
echo "Installing dependencies to: $INSTALL_PREFIX"

mkdir -p deps
cd deps

# Function to build a system dependency
build_system() {
  local name=$1
  local repo=$2
  local branch=$3
  shift 3
  local cmake_args=("$@")

  echo ""
  echo "========================================="
  echo "Building $name..."
  echo "========================================="

  git clone "https://github.com/kcenon/${repo}.git" || true
  if [ ! -d "$repo" ]; then
    echo "ERROR: Failed to clone $repo"
    exit 1
  fi

  cd "$repo"

  # Checkout specific branch if provided
  if [ -n "$branch" ] && [ "$branch" != "main" ]; then
    echo "Checking out branch: $branch"
    git fetch origin "$branch" || true
    git checkout "$branch" || echo "Warning: Could not checkout $branch, using default branch"
  fi

  # Common CMake arguments
  local common_args=(
    -B build
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DBUILD_TESTS=OFF
    -DBUILD_EXAMPLES=OFF
    -DBUILD_SAMPLES=OFF
  )

  # Run CMake configure
  cmake "${common_args[@]}" "${cmake_args[@]}"

  # Build
  cmake --build build --parallel

  # Install
  cmake --install build

  echo "✓ $name built and installed successfully"
  cd ..
}

# Build Tier 0: common_system
build_system "common_system" "common_system" "main"

# Verify common_system installation
if [ ! -d "$INSTALL_PREFIX/include/kcenon/common" ]; then
  echo "ERROR: common_system installation verification failed"
  ls -la "$INSTALL_PREFIX/include" || true
  exit 1
fi

# Build Tier 1: thread_system
build_system "thread_system" "thread_system" "main" \
  -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
  -Dcommon_system_DIR="$INSTALL_PREFIX/lib/cmake/common_system" \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DBUILD_INTEGRATION_TESTS=OFF

# Verify thread_system installation
if [ ! -d "$INSTALL_PREFIX/include/kcenon/thread" ]; then
  echo "ERROR: thread_system installation verification failed"
  ls -la "$INSTALL_PREFIX/include/kcenon" || true
  exit 1
fi

# Build Tier 1: logger_system
build_system "logger_system" "logger_system" "main" \
  -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
  -Dcommon_system_DIR="$INSTALL_PREFIX/lib/cmake/common_system" \
  -Dthread_system_DIR="$INSTALL_PREFIX/lib/cmake/thread_system" \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DBUILD_WITH_THREAD_SYSTEM=ON \
  -DLOGGER_BUILD_INTEGRATION_TESTS=OFF

# Build Tier 1: monitoring_system (using fix branch until PR #76 is merged)
build_system "monitoring_system" "monitoring_system" "fix/cmake-install-paths" \
  -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
  -Dcommon_system_DIR="$INSTALL_PREFIX/lib/cmake/common_system" \
  -Dthread_system_DIR="$INSTALL_PREFIX/lib/cmake/thread_system" \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DBUILD_WITH_THREAD_SYSTEM=ON \
  -DMONITORING_BUILD_INTEGRATION_TESTS=OFF

# Build Tier 1: container_system
build_system "container_system" "container_system" "main" \
  -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
  -Dcommon_system_DIR="$INSTALL_PREFIX/lib/cmake/common_system" \
  -DBUILD_WITH_COMMON_SYSTEM=ON

# Build Tier 2: network_system
build_system "network_system" "network_system" "main" \
  -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
  -Dcommon_system_DIR="$INSTALL_PREFIX/lib/cmake/common_system" \
  -Dthread_system_DIR="$INSTALL_PREFIX/lib/cmake/thread_system" \
  -Dlogger_system_DIR="$INSTALL_PREFIX/lib/cmake/logger_system" \
  -Dmonitoring_system_DIR="$INSTALL_PREFIX/lib/cmake/monitoring_system" \
  -Dcontainer_system_DIR="$INSTALL_PREFIX/lib/cmake/container_system" \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DBUILD_WITH_THREAD_SYSTEM=ON \
  -DBUILD_WITH_LOGGER_SYSTEM=ON \
  -DBUILD_WITH_MONITORING_SYSTEM=ON \
  -DBUILD_WITH_CONTAINER_SYSTEM=ON \
  -DBUILD_INTEGRATION_TESTS=OFF

echo ""
echo "========================================="
echo "All dependencies built successfully!"
echo "========================================="
echo "Installation directory: $INSTALL_PREFIX"
ls -la "$INSTALL_PREFIX/include/kcenon" || true
