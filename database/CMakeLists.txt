##################################################
# Database Module CMakeLists.txt
#
# Builds the database library for the database system.
# Provides database abstraction and PostgreSQL implementation.
##################################################

# Project definition
project(database
    VERSION 1.0.0
    DESCRIPTION "Database abstraction layer with PostgreSQL support"
    LANGUAGES CXX
)

##################################################
# Source Files Configuration
##################################################

# Collect all header files
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/database_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/connection_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql/mysql_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite/sqlite_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb/mongodb_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis/redis_manager.h
    # Phase 4 Headers
    ${CMAKE_CURRENT_SOURCE_DIR}/orm/entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/performance_monitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/security/secure_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/async/async_operations.h
    # Sprint 5 Headers (Backend Plugin System)
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_registry.h
    # Sprint 5.2 Headers (PostgreSQL Backend Plugin)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/postgresql_backend.h
    # Sprint 5.3 Headers (Additional Backend Plugins)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis_backend.h
    # P1-1 Headers (Resilient Connection)
    ${CMAKE_CURRENT_SOURCE_DIR}/resilience/resilient_database_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resilience/connection_health_monitor.h
    # P1-2 Headers (Immutable Query Builder)
    ${CMAKE_CURRENT_SOURCE_DIR}/query/immutable_query_builder.h
)

# Collect all source files
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql/mysql_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite/sqlite_manager.cpp
    # Phase 4 Sources
    ${CMAKE_CURRENT_SOURCE_DIR}/orm/entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/performance_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb/mongodb_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis/redis_manager.cpp
    # Sprint 5 Sources (Backend Plugin System)
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_registry.cpp
    # Sprint 5.2 Sources (PostgreSQL Backend Plugin)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/postgresql_backend.cpp
    # Sprint 5.3 Sources (Additional Backend Plugins)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis_backend.cpp
    # P1-1 Sources (Resilient Connection)
    ${CMAKE_CURRENT_SOURCE_DIR}/resilience/resilient_database_connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resilience/connection_health_monitor.cpp
    # P1-2 Sources (Immutable Query Builder)
    ${CMAKE_CURRENT_SOURCE_DIR}/query/immutable_query_builder.cpp
)

# Add container-based protocol serialization if container_system is available
if(USE_CONTAINER_SYSTEM AND _FOUND_CONTAINER_INCLUDE)
    list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/database_protocol_container.cpp
    )
endif()

# Phase 2: connection_pool_v2 and connection_pool_v3 (added conditionally with thread_system)
if(USE_THREAD_SYSTEM)
    list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/pooling/connection_pool_v2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pooling/connection_pool_v3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pooling/connection_priority_instantiation.cpp
    )
endif()

# P0-3 and P0-4: Database Proxy Server and Remote Client (added conditionally with network_system)
if(USE_NETWORK_SYSTEM)
    list(APPEND SOURCE_FILES
        # P0-3 Sources (Database Proxy Server)
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/database_protocol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/server/database_session.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/server/database_proxy_server.cpp
        # P0-4 Sources (Remote Database Client)
        ${CMAKE_CURRENT_SOURCE_DIR}/client/remote_database_client.cpp
    )
endif()

##################################################
# Library Target Configuration
##################################################

# Create static library
add_library(${PROJECT_NAME} STATIC
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

# Suppress warnings inherited from parent project
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-c++98-compat-extra-semi
        -Wno-extra-semi
        -Wno-unused-private-field
        -Wno-sign-conversion
        -Wno-conversion
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4100  # unreferenced formal parameter
    )
endif()

if(BUILD_WITH_COMMON_SYSTEM)
    message(STATUS "Database: Enabling common_system integration")
    find_system_dependency_include(common_system)

    if(common_system_INCLUDE_DIR)
        message(STATUS "Found common_system at: ${common_system_INCLUDE_DIR}")
        target_include_directories(${PROJECT_NAME} PUBLIC
            $<BUILD_INTERFACE:${common_system_INCLUDE_DIR}>
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC BUILD_WITH_COMMON_SYSTEM)
        message(STATUS "Database: common_system integration enabled")
    else()
        message(WARNING "common_system not found - integration disabled. Install common_system or set BUILD_WITH_COMMON_SYSTEM=OFF to suppress this warning.")
        set(BUILD_WITH_COMMON_SYSTEM OFF CACHE BOOL "common_system not found" FORCE)
    endif()
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add HAS_COROUTINES definition if C++20 coroutines are available
if(HAS_COROUTINES)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_COROUTINES)
endif()

##################################################
# Include Directories
##################################################

# Public include directories (for consumers of this library)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include/database_system>
)

##################################################
# Dependencies
##################################################

# Find required packages
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

# thread_system integration (optional)
if(USE_THREAD_SYSTEM)
    # Try to find thread_system
    find_system_dependency_include(thread_system)
    set(_DATABASE_THREAD_INCLUDE_PATHS ${thread_system_INCLUDE_DIR})

    set(_FOUND_THREAD_INCLUDE FALSE)
    foreach(_path ${_DATABASE_THREAD_INCLUDE_PATHS})
        if(EXISTS "${_path}/kcenon/thread/core/thread_pool.h")
            message(STATUS "Database: Found thread_system headers at: ${_path}")
            target_include_directories(${PROJECT_NAME} PUBLIC
                $<BUILD_INTERFACE:${_path}>
            )
            set(_FOUND_THREAD_INCLUDE TRUE)
            break()
        endif()
    endforeach()

    if(_FOUND_THREAD_INCLUDE)
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_THREAD_SYSTEM)
        message(STATUS "Database: thread_system integration enabled")

        # Store thread_system source directory for typed_pool implementation
        set(_THREAD_SYSTEM_SOURCE "")
        foreach(_path ${_DATABASE_THREAD_INCLUDE_PATHS})
            if(EXISTS "${_path}/kcenon/thread/core/thread_pool.h")
                # Get parent directory (remove /include)
                get_filename_component(_THREAD_SYSTEM_SOURCE "${_path}" DIRECTORY)
                message(STATUS "Database: thread_system source directory: ${_THREAD_SYSTEM_SOURCE}")
                break()
            endif()
        endforeach()

        # thread_system requires fmt
        find_package(fmt CONFIG REQUIRED)
        if(fmt_FOUND)
            target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
            message(STATUS "Database: fmt library found (required by thread_system)")
        else()
            message(FATAL_ERROR "Database: fmt library not found - required by thread_system")
        endif()

        # Add thread_system src directory to include paths for instantiation file
        if(_THREAD_SYSTEM_SOURCE AND EXISTS "${_THREAD_SYSTEM_SOURCE}/src/impl/typed_pool")
            message(STATUS "Database: Configured typed_pool instantiation for connection_priority")

            # Add thread_system src directory so we can #include "impl/typed_pool/*.cpp"
            target_include_directories(${PROJECT_NAME} PRIVATE
                ${_THREAD_SYSTEM_SOURCE}/src
            )
        else()
            message(WARNING "Database: typed_pool implementation not found - connection_pool_v2 may have linking issues")
        endif()

        # Try to link thread_system library if built
        set(_THREAD_LIB_PATHS
            "/Users/$ENV{USER}/Sources/thread_system/build/lib"
            "/home/$ENV{USER}/Sources/thread_system/build/lib"
        )

        foreach(_lib_path ${_THREAD_LIB_PATHS})
            if(EXISTS "${_lib_path}")
                message(STATUS "Database: Found thread_system library path: ${_lib_path}")
                target_link_directories(${PROJECT_NAME} PUBLIC ${_lib_path})
                # Link to thread_system libraries if they exist
                # Note: thread_system is mostly header-only, so this is optional
                break()
            endif()
        endforeach()
    else()
        message(WARNING "Database: thread_system headers not found - integration disabled")
        set(USE_THREAD_SYSTEM OFF CACHE BOOL "thread_system not found" FORCE)
    endif()
endif()

# container_system integration (optional)
if(USE_CONTAINER_SYSTEM)
    # Try to find container_system source directory first
    find_system_dependency(container_system)

    if(container_system_FOUND AND container_system_SOURCE_DIR)
        # Container headers are in the source directory root
        set(_DATABASE_CONTAINER_INCLUDE_PATHS
            "${container_system_SOURCE_DIR}"
            "${container_system_SOURCE_DIR}/include"
        )
    else()
        # Fallback to include search
        find_system_dependency_include(container_system)
        set(_DATABASE_CONTAINER_INCLUDE_PATHS ${container_system_INCLUDE_DIR})
    endif()

    set(_FOUND_CONTAINER_INCLUDE FALSE)
    foreach(_path ${_DATABASE_CONTAINER_INCLUDE_PATHS})
        if(EXISTS "${_path}/core/container.h")
            message(STATUS "Database: Found container_system headers at: ${_path}")
            target_include_directories(${PROJECT_NAME} PUBLIC
                $<BUILD_INTERFACE:${_path}>
            )
            set(_FOUND_CONTAINER_INCLUDE TRUE)
            break()
        endif()
    endforeach()

    if(_FOUND_CONTAINER_INCLUDE)
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_CONTAINER_SYSTEM)
        message(STATUS "Database: container_system integration enabled")

        # Try to link container_system library if built
        set(_CONTAINER_LIB_PATHS
            "/Users/$ENV{USER}/Sources/container_system/build/lib"
            "/home/$ENV{USER}/Sources/container_system/build/lib"
        )

        foreach(_lib_path ${_CONTAINER_LIB_PATHS})
            if(EXISTS "${_lib_path}")
                message(STATUS "Database: Found container_system library path: ${_lib_path}")
                target_link_directories(${PROJECT_NAME} PUBLIC ${_lib_path})
                # Link to container_system library if it exists
                if(EXISTS "${_lib_path}/libcontainer.a" OR EXISTS "${_lib_path}/libcontainer.dylib")
                    target_link_libraries(${PROJECT_NAME} PUBLIC container)
                    message(STATUS "Database: Linked container_system library")
                endif()
                break()
            endif()
        endforeach()
    else()
        message(WARNING "Database: container_system headers not found - integration disabled")
        set(USE_CONTAINER_SYSTEM OFF CACHE BOOL "container_system not found" FORCE)
    endif()
endif()

# network_system integration (optional)
if(USE_NETWORK_SYSTEM)
    # Try to find network_system source directory first
    find_system_dependency(network_system)

    if(network_system_FOUND AND network_system_SOURCE_DIR)
        # Network headers are in the source directory root
        set(_DATABASE_NETWORK_INCLUDE_PATHS
            "${network_system_SOURCE_DIR}"
            "${network_system_SOURCE_DIR}/include"
        )
    else()
        # Fallback to include search
        find_system_dependency_include(network_system)
        set(_DATABASE_NETWORK_INCLUDE_PATHS ${network_system_INCLUDE_DIR})
    endif()

    set(_FOUND_NETWORK_INCLUDE FALSE)
    foreach(_path ${_DATABASE_NETWORK_INCLUDE_PATHS})
        if(EXISTS "${_path}/network_system/core/messaging_server.h" OR EXISTS "${_path}/core/messaging_server.h")
            message(STATUS "Database: Found network_system headers at: ${_path}")
            target_include_directories(${PROJECT_NAME} PUBLIC
                $<BUILD_INTERFACE:${_path}>
            )
            set(_FOUND_NETWORK_INCLUDE TRUE)
            break()
        endif()
    endforeach()

    if(_FOUND_NETWORK_INCLUDE)
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_NETWORK_SYSTEM)
        message(STATUS "Database: network_system integration enabled")

        # Try to link network_system library if built
        set(_NETWORK_LIB_PATHS
            "/Users/$ENV{USER}/Sources/network_system/build/lib"
            "/home/$ENV{USER}/Sources/network_system/build/lib"
        )

        foreach(_lib_path ${_NETWORK_LIB_PATHS})
            if(EXISTS "${_lib_path}")
                message(STATUS "Database: Found network_system library path: ${_lib_path}")
                target_link_directories(${PROJECT_NAME} PUBLIC ${_lib_path})
                # Link to network_system library if it exists
                if(EXISTS "${_lib_path}/libnetwork.a" OR EXISTS "${_lib_path}/libnetwork.dylib")
                    target_link_libraries(${PROJECT_NAME} PUBLIC network)
                    message(STATUS "Database: Linked network_system library")
                endif()
                break()
            endif()
        endforeach()
    else()
        message(WARNING "Database: network_system headers not found - integration disabled")
        set(USE_NETWORK_SYSTEM OFF CACHE BOOL "network_system not found" FORCE)
    endif()
endif()

# PostgreSQL dependency (optional)
if(USE_POSTGRESQL)
    find_package(libpqxx CONFIG QUIET)
    find_package(OpenSSL QUIET)

    if(libpqxx_FOUND AND OPENSSL_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                libpqxx::pqxx
                OpenSSL::SSL
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_POSTGRESQL)
        message(STATUS "PostgreSQL support enabled")
        message(STATUS "  libpqxx found: ${libpqxx_FOUND}")
        message(STATUS "  OpenSSL found: ${OPENSSL_FOUND}")
    else()
        message(WARNING "PostgreSQL libraries not found, disabling PostgreSQL support")
        message(STATUS "  libpqxx found: ${libpqxx_FOUND}")
        message(STATUS "  OpenSSL found: ${OPENSSL_FOUND}")
        set(USE_POSTGRESQL OFF CACHE BOOL "PostgreSQL support disabled due to missing libraries" FORCE)
    endif()
endif()

# MySQL dependency (optional)
if(USE_MYSQL)
    find_package(unofficial-libmysql CONFIG QUIET)

    if(unofficial-libmysql_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                unofficial::libmysql::libmysql
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_MYSQL)
        message(STATUS "MySQL support enabled")
        message(STATUS "  libmysql found: ${unofficial-libmysql_FOUND}")
    else()
        message(WARNING "MySQL libraries not found, disabling MySQL support")
        message(STATUS "  libmysql found: ${unofficial-libmysql_FOUND}")
        set(USE_MYSQL OFF CACHE BOOL "MySQL support disabled due to missing libraries" FORCE)
    endif()
endif()

# SQLite dependency (optional)
if(USE_SQLITE)
    # Try CONFIG mode first (vcpkg)
    find_package(unofficial-sqlite3 CONFIG QUIET)

    if(unofficial-sqlite3_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                unofficial::sqlite3::sqlite3
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SQLITE)
        message(STATUS "SQLite support enabled (CONFIG)")
        message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
    else()
        # Try pkg-config mode (Homebrew, system package managers)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(SQLITE3 QUIET sqlite3)
            if(SQLITE3_FOUND)
                target_include_directories(${PROJECT_NAME} PUBLIC ${SQLITE3_INCLUDE_DIRS})
                target_link_libraries(${PROJECT_NAME} PUBLIC ${SQLITE3_LIBRARIES})
                target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SQLITE)
                message(STATUS "SQLite support enabled (pkg-config)")
                message(STATUS "  sqlite3 version: ${SQLITE3_VERSION}")
                message(STATUS "  sqlite3 include: ${SQLITE3_INCLUDE_DIRS}")
                message(STATUS "  sqlite3 libraries: ${SQLITE3_LIBRARIES}")
            else()
                message(WARNING "SQLite libraries not found, disabling SQLite support")
                message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
                set(USE_SQLITE OFF CACHE BOOL "SQLite support disabled due to missing libraries" FORCE)
            endif()
        else()
            message(WARNING "SQLite libraries not found, disabling SQLite support")
            message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
            set(USE_SQLITE OFF CACHE BOOL "SQLite support disabled due to missing libraries" FORCE)
        endif()
    endif()
endif()

# MongoDB dependency (optional)
if(USE_MONGODB)
    find_package(mongocxx CONFIG QUIET)
    find_package(bsoncxx CONFIG QUIET)

    if(mongocxx_FOUND AND bsoncxx_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                mongo::mongocxx_shared
                mongo::bsoncxx_shared
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_MONGODB)
        message(STATUS "MongoDB support enabled")
        message(STATUS "  mongocxx found: ${mongocxx_FOUND}")
        message(STATUS "  bsoncxx found: ${bsoncxx_FOUND}")
    else()
        message(WARNING "MongoDB libraries not found, disabling MongoDB support")
        message(STATUS "  mongocxx found: ${mongocxx_FOUND}")
        message(STATUS "  bsoncxx found: ${bsoncxx_FOUND}")
        set(USE_MONGODB OFF CACHE BOOL "MongoDB support disabled due to missing libraries" FORCE)
    endif()
endif()

# Redis dependency (optional)
if(USE_REDIS)
    find_package(hiredis CONFIG QUIET)

    if(hiredis_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                hiredis::hiredis
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_REDIS)
        message(STATUS "Redis support enabled")
        message(STATUS "  hiredis found: ${hiredis_FOUND}")
    else()
        message(WARNING "Redis libraries not found, disabling Redis support")
        message(STATUS "  hiredis found: ${hiredis_FOUND}")
        set(USE_REDIS OFF CACHE BOOL "Redis support disabled due to missing libraries" FORCE)
    endif()
endif()

##################################################
# Compiler Options
##################################################

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX-
    )
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7 or later
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

##################################################
# Testing Support
##################################################

# Enable testing features if building tests
if(USE_UNIT_TEST AND NOT BUILD_DATABASE_SYSTEM_AS_SUBMODULE)
    # Add any database-specific test configurations here
    set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)
endif()

##################################################
# Integrated Database System (Adapter Pattern)
##################################################

if(BUILD_INTEGRATED_DATABASE)
    add_subdirectory(integrated)
    message(STATUS "Building integrated database system with adapter pattern")
endif()

##################################################
# Summary
##################################################

message(STATUS "Database library configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Headers: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Dependencies: Threads, libpqxx (if enabled), libmysql (if enabled)")
