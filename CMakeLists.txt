cmake_minimum_required(VERSION 3.16)

##################################################
# Database System CMakeLists.txt
#
# Independent database system with multi-backend support
# Provides database abstraction and PostgreSQL implementation.
##################################################

# Project definition
project(database_system
    VERSION 1.0.0
    DESCRIPTION "Advanced C++20 Database System with Multi-Backend Support"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(USE_UNIT_TEST "Use unit test" ON)
option(BUILD_DATABASE_SAMPLES "Build database system samples" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_MYSQL "Enable MySQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(BUILD_DATABASE "Build database module" ON)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration" ON)
option(USE_THREAD_SYSTEM "Enable thread_system integration for high-performance threading" ON)
option(DATABASE_BUILD_BENCHMARKS "Build database system benchmarks" OFF)
option(DATABASE_BUILD_INTEGRATION_TESTS "Build database system integration tests" ON)

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

# Find required packages
find_package(Threads REQUIRED)

# thread_system integration (optional)
if(USE_THREAD_SYSTEM)
    message(STATUS "Searching for thread_system...")

    # Try to find thread_system with priority order:
    # 1. macOS: /Users/${USER}/Sources/thread_system
    # 2. Linux: /home/${USER}/Sources/thread_system
    # 3. CONFIG mode (installed or vcpkg)
    set(_DATABASE_THREAD_PATHS
        "/Users/$ENV{USER}/Sources/thread_system"
        "/home/$ENV{USER}/Sources/thread_system"
    )

    set(_FOUND_THREAD_SYSTEM FALSE)
    foreach(_path ${_DATABASE_THREAD_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt")
            message(STATUS "Found thread_system source at: ${_path}")
            set(thread_system_DIR "${_path}/build" CACHE PATH "thread_system build directory")
            set(_FOUND_THREAD_SYSTEM TRUE)
            break()
        endif()
    endforeach()

    # Try CONFIG mode if not found in source paths
    if(NOT _FOUND_THREAD_SYSTEM)
        find_package(thread_system CONFIG QUIET)
        if(thread_system_FOUND)
            set(_FOUND_THREAD_SYSTEM TRUE)
            message(STATUS "Found thread_system via CONFIG mode")
        endif()
    endif()

    if(_FOUND_THREAD_SYSTEM)
        message(STATUS "thread_system integration enabled")
    else()
        message(WARNING "thread_system not found - integration disabled. Install thread_system or set USE_THREAD_SYSTEM=OFF to suppress this warning.")
        set(USE_THREAD_SYSTEM OFF CACHE BOOL "thread_system not found" FORCE)
    endif()
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
message(STATUS "    - thread_system integration: ${USE_THREAD_SYSTEM}")
message(STATUS "    - Build samples: ${BUILD_DATABASE_SAMPLES}")
message(STATUS "    - Build tests: ${USE_UNIT_TEST}")
message(STATUS "    - Build integration tests: ${DATABASE_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Output directories:")
message(STATUS "    - Runtime: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "    - Library: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "    - Archive: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
