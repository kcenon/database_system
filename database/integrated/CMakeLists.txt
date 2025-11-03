##################################################
# Integrated Database System CMakeLists.txt
#
# Builds the integrated database system with adapter pattern
# for logger_system, monitoring_system, thread_system integration
##################################################

cmake_minimum_required(VERSION 3.16)

# Library name
set(INTEGRATED_DB_TARGET integrated_database)

##################################################
# Source Files
##################################################

set(INTEGRATED_DB_HEADERS
    # Core
    core/configuration.h
    # Adapters
    adapters/logger_adapter.h
    adapters/monitoring_adapter.h
    # Note: coordinator headers will be added in later phases
)

set(INTEGRATED_DB_SOURCES
    # Adapters
    adapters/logger_adapter.cpp
    adapters/monitoring_adapter.cpp
    # Note: more implementation files will be added in later phases
)

##################################################
# Create Library Target
##################################################

if(INTEGRATED_DB_SOURCES)
    # If we have source files, create a regular library
    add_library(${INTEGRATED_DB_TARGET} ${INTEGRATED_DB_SOURCES})

    target_include_directories(${INTEGRATED_DB_TARGET}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For #include "integrated/..."
            $<INSTALL_INTERFACE:include>
    )

    # Add common_system include path if available
    if(BUILD_WITH_COMMON_SYSTEM)
        target_include_directories(${INTEGRATED_DB_TARGET}
            PUBLIC
                $<BUILD_INTERFACE:/Users/$ENV{USER}/Sources/common_system/include>
                $<BUILD_INTERFACE:/home/$ENV{USER}/Sources/common_system/include>
        )
    endif()

    # Add logger_system include path if available
    if(USE_LOGGER_SYSTEM)
        target_include_directories(${INTEGRATED_DB_TARGET}
            PUBLIC
                $<BUILD_INTERFACE:/Users/$ENV{USER}/Sources/logger_system/include>
                $<BUILD_INTERFACE:/home/$ENV{USER}/Sources/logger_system/include>
                # logger_system needs thread_system headers when built with USE_THREAD_SYSTEM_INTEGRATION
                $<BUILD_INTERFACE:/Users/$ENV{USER}/Sources/thread_system/include>
                $<BUILD_INTERFACE:/home/$ENV{USER}/Sources/thread_system/include>
        )
        # logger_system was built with USE_THREAD_SYSTEM_INTEGRATION, so we need to define it too
        target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_THREAD_SYSTEM_INTEGRATION)
    endif()

    # Add monitoring_system include path if available
    if(USE_MONITORING_SYSTEM)
        target_include_directories(${INTEGRATED_DB_TARGET}
            PUBLIC
                $<BUILD_INTERFACE:/Users/$ENV{USER}/Sources/monitoring_system/include>
                $<BUILD_INTERFACE:/home/$ENV{USER}/Sources/monitoring_system/include>
        )
    endif()
else()
    # Header-only for now (Phase 1)
    add_library(${INTEGRATED_DB_TARGET} INTERFACE)

    target_include_directories(${INTEGRATED_DB_TARGET}
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<INSTALL_INTERFACE:include>
    )
endif()

##################################################
# Compiler Definitions
##################################################

# Common system integration (almost always enabled)
if(BUILD_WITH_COMMON_SYSTEM)
    if(TARGET ${INTEGRATED_DB_TARGET})
        if(INTEGRATED_DB_SOURCES)
            target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_COMMON_SYSTEM)
        else()
            target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE USE_COMMON_SYSTEM)
        endif()
    endif()
    message(STATUS "Integrated DB: common_system integration enabled")
endif()

# Thread system integration
if(USE_THREAD_SYSTEM)
    if(TARGET ${INTEGRATED_DB_TARGET})
        if(INTEGRATED_DB_SOURCES)
            target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_THREAD_SYSTEM)
        else()
            target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE USE_THREAD_SYSTEM)
        endif()
    endif()
    message(STATUS "Integrated DB: thread_system integration enabled")
endif()

# Logger system integration
if(USE_LOGGER_SYSTEM)
    if(TARGET ${INTEGRATED_DB_TARGET})
        if(INTEGRATED_DB_SOURCES)
            target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_LOGGER_SYSTEM)
        else()
            target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE USE_LOGGER_SYSTEM)
        endif()
    endif()
    message(STATUS "Integrated DB: logger_system integration enabled")
endif()

# Monitoring system integration
if(USE_MONITORING_SYSTEM)
    if(TARGET ${INTEGRATED_DB_TARGET})
        if(INTEGRATED_DB_SOURCES)
            target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_MONITORING_SYSTEM)
        else()
            target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE USE_MONITORING_SYSTEM)
        endif()
    endif()
    message(STATUS "Integrated DB: monitoring_system integration enabled")
endif()

##################################################
# Dependencies
##################################################

# Link external systems (if found)

# common_system (optional - only link if target exists)
if(BUILD_WITH_COMMON_SYSTEM AND TARGET common_system)
    if(INTEGRATED_DB_SOURCES)
        target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC common_system)
    else()
        target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE common_system)
    endif()
    message(STATUS "Integrated DB: Linked with common_system")
else()
    message(STATUS "Integrated DB: common_system not available - compilation definitions only")
endif()

# thread_system
if(USE_THREAD_SYSTEM AND TARGET thread_system)
    if(INTEGRATED_DB_SOURCES)
        target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC thread_system)
    else()
        target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE thread_system)
    endif()
    message(STATUS "Integrated DB: Linked with thread_system")
endif()

# logger_system
if(USE_LOGGER_SYSTEM)
    # Try to find logger_system library
    if(TARGET logger_system)
        if(INTEGRATED_DB_SOURCES)
            target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC logger_system)
        else()
            target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE logger_system)
        endif()
        message(STATUS "Integrated DB: Linked with logger_system target")
    else()
        # Try to link directly with LoggerSystem library
        set(_LOGGER_LIB_PATHS
            "/Users/$ENV{USER}/Sources/logger_system/build/lib"
            "/home/$ENV{USER}/Sources/logger_system/build/lib"
        )

        foreach(_path ${_LOGGER_LIB_PATHS})
            if(EXISTS "${_path}/libLoggerSystem.a")
                if(INTEGRATED_DB_SOURCES)
                    target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC
                        "${_path}/libLoggerSystem.a"
                        "${_path}/libthread_base.a"
                        "${_path}/libutilities.a"
                        "${_path}/libinterfaces.a"
                    )
                else()
                    target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE
                        "${_path}/libLoggerSystem.a"
                        "${_path}/libthread_base.a"
                        "${_path}/libutilities.a"
                        "${_path}/libinterfaces.a"
                    )
                endif()
                message(STATUS "Integrated DB: Linked with LoggerSystem library at ${_path}")
                break()
            endif()
        endforeach()
    endif()
endif()

# monitoring_system
if(USE_MONITORING_SYSTEM AND TARGET monitoring_system)
    if(INTEGRATED_DB_SOURCES)
        target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC monitoring_system)
    else()
        target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE monitoring_system)
    endif()
    message(STATUS "Integrated DB: Linked with monitoring_system")
endif()

##################################################
# C++ Standard
##################################################

if(INTEGRATED_DB_SOURCES)
    target_compile_features(${INTEGRATED_DB_TARGET} PUBLIC cxx_std_20)
else()
    target_compile_features(${INTEGRATED_DB_TARGET} INTERFACE cxx_std_20)
endif()

##################################################
# Installation
##################################################

install(TARGETS ${INTEGRATED_DB_TARGET}
    EXPORT database_system-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY core/ adapters/
    DESTINATION include/database_system/database/integrated
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

message(STATUS "Integrated database system configured")
