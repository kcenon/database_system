cmake_minimum_required(VERSION 3.16)

##################################################
# Database System CMakeLists.txt
#
# Independent database system with multi-backend support
# Provides database abstraction and PostgreSQL implementation.
##################################################

# Project definition
project(database_system
    VERSION 2.0.0
    DESCRIPTION "Advanced Distributed C++17/20 Database System with Full Network Integration"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Detect C++20 coroutine support (optional feature)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <coroutine>
    struct task {
        struct promise_type {
            task get_return_object() { return {}; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };
    };
    task foo() { co_return; }
    int main() {}
" HAS_COROUTINES)

if(HAS_COROUTINES)
    message(STATUS "C++20 coroutines available - async features enabled")
else()
    message(STATUS "C++20 coroutines not available - async features disabled")
endif()

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(USE_UNIT_TEST "Use unit test" ON)
option(BUILD_DATABASE_SAMPLES "Build database system samples" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_MYSQL "Enable MySQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(BUILD_DATABASE "Build database module" ON)
option(USE_THREAD_SYSTEM "Enable thread_system integration for high-performance threading" ON)
option(USE_LOGGER_SYSTEM "Enable logger_system integration for advanced logging" ON)
option(USE_MONITORING_SYSTEM "Enable monitoring_system integration for metrics and profiling" ON)
option(USE_CONTAINER_SYSTEM "Enable container_system integration for high-performance serialization" ON)
option(BUILD_INTEGRATED_DATABASE "Build integrated database system with adapter pattern" ON)
option(DATABASE_BUILD_BENCHMARKS "Build database system benchmarks" OFF)
option(DATABASE_BUILD_INTEGRATION_TESTS "Build database system integration tests" ON)

# Required integrations (no longer optional)
set(BUILD_WITH_COMMON_SYSTEM ON)  # Always required
set(USE_NETWORK_SYSTEM ON)  # Now required for distributed features

# Respect global BUILD_INTEGRATION_TESTS flag if set
if(DEFINED BUILD_INTEGRATION_TESTS)
    if(BUILD_INTEGRATION_TESTS)
        set(_DATABASE_BUILD_IT_VALUE ON)
    else()
        set(_DATABASE_BUILD_IT_VALUE OFF)
    endif()
    set(DATABASE_BUILD_INTEGRATION_TESTS ${_DATABASE_BUILD_IT_VALUE} CACHE BOOL "Build database system integration tests" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00) # Windows 10
elseif(APPLE)
    add_definitions(-DAPPLE_PLATFORM)
endif()

# Include CMake utilities
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FindSystemDependency)

# Find required packages
find_package(Threads REQUIRED)

# common_system integration (strongly recommended for Result<T> pattern)
message(STATUS "Searching for common_system...")
find_system_dependency(common_system)

if(common_system_FOUND)
    message(STATUS "common_system integration enabled")
else()
    message(WARNING "common_system not found - using fallback Result<T> implementation. "
                    "common_system is STRONGLY RECOMMENDED for database_system v2.0+. "
                    "Install common_system or set BUILD_WITH_COMMON_SYSTEM=OFF to suppress this warning.")
    set(BUILD_WITH_COMMON_SYSTEM OFF CACHE BOOL "common_system not found" FORCE)
endif()

# thread_system integration (optional)
if(USE_THREAD_SYSTEM)
    message(STATUS "Searching for thread_system...")
    find_system_dependency(thread_system)

    if(thread_system_FOUND)
        message(STATUS "thread_system integration enabled")
    else()
        message(WARNING "thread_system not found - integration disabled. Install thread_system or set USE_THREAD_SYSTEM=OFF to suppress this warning.")
        set(USE_THREAD_SYSTEM OFF CACHE BOOL "thread_system not found" FORCE)
    endif()
endif()

# logger_system integration (optional)
if(USE_LOGGER_SYSTEM)
    message(STATUS "Searching for logger_system...")
    find_system_dependency(logger_system)

    if(logger_system_FOUND)
        message(STATUS "logger_system integration enabled")
    else()
        message(WARNING "logger_system not found - integration disabled. Install logger_system or set USE_LOGGER_SYSTEM=OFF to suppress this warning.")
        set(USE_LOGGER_SYSTEM OFF CACHE BOOL "logger_system not found" FORCE)
    endif()
endif()

# monitoring_system integration (optional)
if(USE_MONITORING_SYSTEM)
    message(STATUS "Searching for monitoring_system...")
    find_system_dependency(monitoring_system)

    if(monitoring_system_FOUND)
        message(STATUS "monitoring_system integration enabled")
    else()
        message(WARNING "monitoring_system not found - integration disabled. Install monitoring_system or set USE_MONITORING_SYSTEM=OFF to suppress this warning.")
        set(USE_MONITORING_SYSTEM OFF CACHE BOOL "monitoring_system not found" FORCE)
    endif()
endif()

# container_system integration (optional)
if(USE_CONTAINER_SYSTEM)
    message(STATUS "Searching for container_system...")
    find_system_dependency(container_system)

    if(container_system_FOUND)
        message(STATUS "container_system integration enabled")
    else()
        message(WARNING "container_system not found - integration disabled. Install container_system or set USE_CONTAINER_SYSTEM=OFF to suppress this warning.")
        set(USE_CONTAINER_SYSTEM OFF CACHE BOOL "container_system not found" FORCE)
    endif()
endif()

# network_system integration (strongly recommended for distributed features)
message(STATUS "Searching for network_system...")
find_system_dependency(network_system)

if(network_system_FOUND)
    message(STATUS "network_system integration enabled")
else()
    message(WARNING "network_system not found - remote database features will be disabled. "
                    "network_system is STRONGLY RECOMMENDED for database_system v2.0+. "
                    "Install network_system or set USE_NETWORK_SYSTEM=OFF to suppress this warning.")
    set(USE_NETWORK_SYSTEM OFF CACHE BOOL "network_system not found" FORCE)
endif()

##################################################
# Modular Architecture
##################################################

# Database module
if(BUILD_DATABASE)
    add_subdirectory(database)
    message(STATUS "Database module will be built")
else()
    message(STATUS "Database module disabled")
endif()

##################################################
# Samples
##################################################

if(BUILD_DATABASE_SAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples)
    add_subdirectory(samples)
    message(STATUS "Database samples will be built")
endif()

##################################################
# Testing
##################################################

# Unit tests
if(USE_UNIT_TEST AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Database tests will be built")
endif()

##################################################
# Benchmarks
##################################################

if(DATABASE_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
    message(STATUS "Database benchmarks will be built")
endif()

##################################################
# Integration Tests
##################################################

if(DATABASE_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests)
    add_subdirectory(integration_tests)
    message(STATUS "Database integration tests will be built")
endif()

##################################################
# Installation Rules
##################################################

include(GNUInstallDirs)

# Install header files
if(BUILD_DATABASE)
    install(DIRECTORY database/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/database_system/database
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
        PATTERN "tests" EXCLUDE
        PATTERN "samples" EXCLUDE
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "Database System configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Features:")
message(STATUS "    - PostgreSQL support: ${USE_POSTGRESQL}")
message(STATUS "    - MySQL support: ${USE_MYSQL}")
message(STATUS "    - SQLite support: ${USE_SQLITE}")
message(STATUS "  Integration:")
message(STATUS "    - common_system: ${BUILD_WITH_COMMON_SYSTEM}")
message(STATUS "    - thread_system: ${USE_THREAD_SYSTEM}")
message(STATUS "    - logger_system: ${USE_LOGGER_SYSTEM}")
message(STATUS "    - monitoring_system: ${USE_MONITORING_SYSTEM}")
message(STATUS "    - container_system: ${USE_CONTAINER_SYSTEM}")
message(STATUS "    - Integrated database system: ${BUILD_INTEGRATED_DATABASE}")
message(STATUS "  Build:")
message(STATUS "    - Build samples: ${BUILD_DATABASE_SAMPLES}")
message(STATUS "    - Build tests: ${USE_UNIT_TEST}")
message(STATUS "    - Build integration tests: ${DATABASE_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Output directories:")
message(STATUS "    - Runtime: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "    - Library: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "    - Archive: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
