cmake_minimum_required(VERSION 3.16)

##################################################
# Database System Project
# Independent, reusable database component
##################################################

project(DatabaseSystem 
    VERSION 1.0.0
    DESCRIPTION "Database abstraction layer with PostgreSQL support"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_DATABASE_TESTS "Build database tests" ON)
option(BUILD_DATABASE_SAMPLES "Build database samples" ON)
option(BUILD_DATABASE_DOCS "Build database documentation" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)

##################################################
# External Dependencies Management
##################################################

# Option to use external container_system
option(USE_EXTERNAL_CONTAINER_SYSTEM "Use external container system" ON)

if(USE_EXTERNAL_CONTAINER_SYSTEM)
    # Try to find external ContainerSystem
    find_package(ContainerSystem QUIET)
    if(ContainerSystem_FOUND)
        message(STATUS "Found external ContainerSystem: ${ContainerSystem_VERSION}")
        set(CONTAINER_TARGET ContainerSystem::container)
    else()
        # Try to find local container_system
        find_path(CONTAINER_SYSTEM_DIR
            NAMES container.h
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/../container_system
                ${CMAKE_PREFIX_PATH}/include/container_system
            NO_DEFAULT_PATH
        )
        
        if(CONTAINER_SYSTEM_DIR)
            message(STATUS "Found local container_system at: ${CONTAINER_SYSTEM_DIR}")
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../container_system container_system)
            set(CONTAINER_TARGET container)
        else()
            message(FATAL_ERROR "ContainerSystem is required but not found")
        endif()
    endif()
else()
    message(FATAL_ERROR "External container system is required")
endif()

# Option to use external thread_system utilities
option(USE_EXTERNAL_THREAD_UTILITIES "Use external thread utilities" ON)

if(USE_EXTERNAL_THREAD_UTILITIES)
    find_path(THREAD_UTILITIES_INCLUDE_DIR
        NAMES utilities/conversion/convert_string.h
        PATHS
            ${CMAKE_PREFIX_PATH}/include/thread_system
            ${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/sources
            ${CMAKE_CURRENT_SOURCE_DIR}/../messaging_system/thread_system/sources
        NO_DEFAULT_PATH
    )
    
    if(THREAD_UTILITIES_INCLUDE_DIR)
        message(STATUS "Found thread utilities at: ${THREAD_UTILITIES_INCLUDE_DIR}")
        set(THREAD_UTILITIES_FOUND TRUE)
    else()
        message(WARNING "Thread utilities not found - some features may be limited")
        set(THREAD_UTILITIES_FOUND FALSE)
    endif()
else()
    set(THREAD_UTILITIES_FOUND FALSE)
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

# Create library
if(BUILD_SHARED_LIBS)
    add_library(database SHARED ${HEADER_FILES} ${SOURCE_FILES})
else()
    add_library(database STATIC ${HEADER_FILES} ${SOURCE_FILES})
endif()

# Create namespace alias
add_library(DatabaseSystem::database ALIAS database)

# Set target properties
set_target_properties(database PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

##################################################
# Include Directories
##################################################

# Public include directories
target_include_directories(database PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/database_system>
)

# Thread utilities include (if available)
if(THREAD_UTILITIES_FOUND)
    target_include_directories(database PRIVATE
        ${THREAD_UTILITIES_INCLUDE_DIR}
    )
    target_compile_definitions(database PRIVATE
        HAS_THREAD_UTILITIES
    )
endif()

##################################################
# Dependencies
##################################################

# Standard dependencies
target_link_libraries(database PUBLIC 
    PostgreSQL::PostgreSQL
    ${CONTAINER_TARGET}
    Threads::Threads
)

##################################################
# Compile Definitions
##################################################

target_compile_definitions(database PRIVATE
    DATABASE_INTERNAL_FEATURES
)

# Thread utilities feature flag
if(THREAD_UTILITIES_FOUND)
    target_compile_definitions(database PUBLIC
        DATABASE_HAS_THREAD_UTILITIES
    )
endif()

# PostgreSQL-specific definitions
if(PostgreSQL_VERSION_STRING VERSION_GREATER_EQUAL "14.0")
    target_compile_definitions(database PRIVATE POSTGRESQL_14_OR_NEWER)
endif()

##################################################
# Compiler Options
##################################################

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(database PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(MSVC)
    target_compile_options(database PRIVATE
        /W4
        /WX-
        /wd4100  # unreferenced formal parameter
        /wd4996  # deprecated functions
    )
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(database PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

##################################################
# Installation Rules
##################################################

include(GNUInstallDirs)

# Install library
install(TARGETS database
    EXPORT DatabaseSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${HEADER_FILES}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/database_system
)

##################################################
# Package Configuration (for find_package)
##################################################

include(CMakePackageConfigHelpers)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DatabaseSystemConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DatabaseSystemConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DatabaseSystem
)

write_basic_package_version_file(
    DatabaseSystemConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DatabaseSystemConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DatabaseSystemConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DatabaseSystem
)

install(EXPORT DatabaseSystemTargets
    FILE DatabaseSystemTargets.cmake
    NAMESPACE DatabaseSystem::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DatabaseSystem
)

##################################################
# Summary
##################################################

message(STATUS "DatabaseSystem configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  PostgreSQL: ${PostgreSQL_VERSION_STRING}")
message(STATUS "  Container system: ${CONTAINER_TARGET}")
message(STATUS "  Thread utilities: ${THREAD_UTILITIES_FOUND}")
message(STATUS "  Tests: ${BUILD_DATABASE_TESTS}")
message(STATUS "  Samples: ${BUILD_DATABASE_SAMPLES}")
message(STATUS "  Documentation: ${BUILD_DATABASE_DOCS}")