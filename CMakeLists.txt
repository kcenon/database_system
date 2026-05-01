cmake_minimum_required(VERSION 3.20)

##################################################
# Database System CMakeLists.txt
#
# Tier 3: Database Layer
# Pure, lightweight Core DAL library with multi-backend support.
# Provides database abstraction and PostgreSQL implementation.
#
# Dependency Chain:
#   database_system (Tier 3)
#     ├── common_system (Tier 0) [REQUIRED] - ILogger, GlobalLoggerRegistry, LOG_* macros
#     ├── thread_system (Tier 1) [OPTIONAL]
#     ├── monitoring_system (Tier 2) [OPTIONAL]
#     └── container_system (Tier 1) [OPTIONAL]
#
# Composition: this top-level file only declares the project, registers the
# cmake/ module path, and includes focused modules in dependency order.
# Per-concern logic lives under cmake/*.cmake (see issue #581).
##################################################

# Project definition
project(database_system
    VERSION 1.0.0
    DESCRIPTION "Pure, Lightweight C++20 Core DAL Library"
    LANGUAGES CXX
)

# Make cmake/*.cmake modules discoverable via include(<name>) and find_package()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build options must come first so subsequent modules can branch on them.
include(cmake/options.cmake)

# Compiler / language settings (C++ standard, output dirs, coverage, platform).
include(cmake/compiler.cmake)

# External and ecosystem dependencies (Threads, OpenSSL, ASIO, common_system, ...).
include(cmake/dependencies.cmake)

##################################################
# Modular Architecture
##################################################

# Database module
if(BUILD_DATABASE)
    add_subdirectory(src)
    message(STATUS "Database module will be built")
else()
    message(STATUS "Database module disabled")
endif()

# Samples and examples
include(cmake/samples.cmake)

# Tests, benchmarks, and integration tests
include(cmake/tests.cmake)

# Installation rules (must come after add_subdirectory(src) so the `database`
# target is defined when install(TARGETS database ...) is evaluated).
include(cmake/install.cmake)

# Build configuration summary (must come last so all *_FOUND flags are populated).
include(cmake/summary.cmake)
