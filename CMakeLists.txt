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

# vcpkg integration
if(USE_POSTGRESQL)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
endif()

##################################################
# Source Files Configuration
##################################################

# Collect all header files
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/database_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.h
)

# Collect all source files
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.cpp
)

##################################################
# Library Target Configuration
##################################################

# Create static library
add_library(database_system STATIC 
    ${HEADER_FILES} 
    ${SOURCE_FILES}
)

# Set target properties
set_target_properties(database_system PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

##################################################
# Include Directories
##################################################

# Public include directories (for consumers of this library)
target_include_directories(database_system PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

##################################################
# Dependencies
##################################################

target_link_libraries(database_system
    PUBLIC
        Threads::Threads
)

if(USE_POSTGRESQL)
    target_link_libraries(database_system
        PUBLIC
            libpqxx::pqxx
            OpenSSL::SSL
    )
    target_compile_definitions(database_system PUBLIC USE_POSTGRESQL)
endif()

##################################################
# Compiler Options
##################################################

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(database_system PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(MSVC)
    target_compile_options(database_system PRIVATE
        /W4
        /WX-
    )
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(database_system PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7 or later
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

##################################################
# PostgreSQL Configuration
##################################################

# Display PostgreSQL configuration
message(STATUS "PostgreSQL Configuration:")
message(STATUS "  Version: ${PostgreSQL_VERSION_STRING}")
message(STATUS "  Include: ${PostgreSQL_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${PostgreSQL_LIBRARIES}")

# Add PostgreSQL-specific definitions if needed
if(PostgreSQL_VERSION_STRING VERSION_GREATER_EQUAL "14.0")
    target_compile_definitions(database_system PRIVATE POSTGRESQL_14_OR_NEWER)
endif()

##################################################
# Installation Rules (when not building as submodule)
##################################################

# Installation is handled by parent CMakeLists.txt

# Export configuration is handled by parent CMakeLists.txt

##################################################
# Testing Support
##################################################

# Enable testing features if building tests
if(BUILD_UNIT_TESTS AND NOT BUILD_MESSAGING_SYSTEM_AS_SUBMODULE)
    # Add any database-specific test configurations here
    set_property(TARGET database_system PROPERTY ENABLE_EXPORTS ON)
endif()

##################################################
# Summary
##################################################

message(STATUS "Database library configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Headers: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Dependencies: PostgreSQL, container, utilities")