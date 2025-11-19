cmake_minimum_required(VERSION 3.16)

##################################################
# Database System Samples CMakeLists.txt
#
# Sample programs demonstrating database system capabilities
##################################################

project(database_system_samples
    VERSION 1.0.0
    DESCRIPTION "Database System Sample Programs"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# vcpkg integration for fmt
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
    message(WARNING "fmt library not found - samples will use basic iostream")
    # Define USE_IOSTREAM when fmt is not available
    add_compile_definitions(USE_IOSTREAM)
endif()

##################################################
# Sample Programs Configuration
##################################################

# List of all sample programs
# Note: Advanced samples excluded - require API features not yet fully implemented
set(SAMPLE_PROGRAMS
    basic_usage
    postgres_advanced
    connection_pool_demo
    # async_operations_demo  # Excluded: requires async_executor API not yet implemented
    # orm_framework_demo  # Excluded: requires ORM framework API not yet implemented
    # performance_monitoring_demo  # Excluded: requires performance monitoring API updates
    # security_framework_demo  # Excluded: requires RBAC/audit APIs not yet implemented
    run_all_samples
)

# Migration samples (thread_system integration demos)
if(USE_THREAD_SYSTEM)
    # Helper function to configure migration samples
    function(configure_migration_sample SAMPLE_NAME SOURCE_FILE)
        add_executable(${SAMPLE_NAME} ${SOURCE_FILE})

        set_target_properties(${SAMPLE_NAME} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        target_include_directories(${SAMPLE_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/../..
        )

        target_link_libraries(${SAMPLE_NAME} PRIVATE
            database
            Threads::Threads
        )

        # Link thread_system libraries
        set(_THREAD_LIB_PATHS
            "/Users/$ENV{USER}/Sources/thread_system/build/lib"
            "/home/$ENV{USER}/Sources/thread_system/build/lib"
        )

        foreach(_lib_path ${_THREAD_LIB_PATHS})
            if(EXISTS "${_lib_path}")
                target_link_directories(${SAMPLE_NAME} PRIVATE ${_lib_path})
                target_link_libraries(${SAMPLE_NAME} PRIVATE
                    ThreadSystem
                )
                break()
            endif()
        endforeach()

        # thread_system requires fmt
        if(fmt_FOUND)
            target_link_libraries(${SAMPLE_NAME} PRIVATE fmt::fmt)
        else()
            message(WARNING "fmt not found - ${SAMPLE_NAME} requires fmt for thread_system")
        endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${SAMPLE_NAME} PRIVATE
                -Wall -Wextra -Wpedantic -Wno-unused-parameter
            )
        endif()

        message(STATUS "Migration sample configured: ${SAMPLE_NAME} (thread_system)")
    endfunction()

    # Phase 1: Async Executor
    configure_migration_sample(async_executor_v2_demo migration/async_executor_v2_demo.cpp)

    # Phase 2: Connection Pool with Priority Scheduling
    configure_migration_sample(connection_pool_v2_demo migration/connection_pool_v2_demo.cpp)

    # Simple test for debugging
    configure_migration_sample(test_simple_v2 migration/test_simple_v2.cpp)

    # Phase 3: Metrics test
    configure_migration_sample(test_metrics_v2 migration/test_metrics_v2.cpp)
endif()

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

##################################################
# Build Each Sample Program
##################################################

foreach(SAMPLE ${SAMPLE_PROGRAMS})
    # Create executable for each sample with unique prefix
    add_executable(database_${SAMPLE} ${SAMPLE}.cpp)
    
    # Set target properties
    set_target_properties(database_${SAMPLE} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Include directories
    target_include_directories(database_${SAMPLE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )

    # Link libraries
    target_link_libraries(database_${SAMPLE} PRIVATE
        database
        Threads::Threads
    )

    # Link fmt only if available
    if(fmt_FOUND)
        target_link_libraries(database_${SAMPLE} PRIVATE fmt::fmt)
    else()
        target_compile_definitions(database_${SAMPLE} PRIVATE USE_IOSTREAM)
    endif()
    
    # Compiler-specific options
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(database_${SAMPLE} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    elseif(MSVC)
        target_compile_options(database_${SAMPLE} PRIVATE
            /W4
            /wd4100  # unreferenced formal parameter
        )
    endif()
    
    # Platform-specific definitions
    if(WIN32)
        target_compile_definitions(database_${SAMPLE} PRIVATE
            _WIN32_WINNT=0x0601
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()
    
    message(STATUS "Database sample configured: ${SAMPLE}")
endforeach()

##################################################
# Installation (Optional)
##################################################

# Create list of prefixed sample targets for installation
set(PREFIXED_SAMPLE_PROGRAMS)
foreach(SAMPLE ${SAMPLE_PROGRAMS})
    list(APPEND PREFIXED_SAMPLE_PROGRAMS database_${SAMPLE})
endforeach()

# Install sample binaries
install(TARGETS ${PREFIXED_SAMPLE_PROGRAMS}
    RUNTIME DESTINATION bin/samples
    COMPONENT samples
)

# Install sample source files for reference
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/database_basic_usage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_advanced.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection_pool_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/database_run_all_samples.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION share/database_system/samples
    COMPONENT samples
)

##################################################
# Integrated Samples (Phase 6)
##################################################

# Add integrated samples subdirectory if building integrated system
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integrated/CMakeLists.txt)
    add_subdirectory(integrated)
    message(STATUS "Integrated samples subdirectory added")
endif()

##################################################
# Summary
##################################################

message(STATUS "Database System Samples configured:")
message(STATUS "  Sample programs: ${SAMPLE_PROGRAMS}")
message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Dependencies: database, fmt (optional), Threads")