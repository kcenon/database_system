cmake_minimum_required(VERSION 3.20)

##################################################
# Database System Examples CMakeLists.txt
#
# Introductory examples demonstrating database_system usage.
# Each example focuses on a single feature area.
##################################################

project(database_system_examples
    VERSION 0.1.0
    DESCRIPTION "Database System Usage Examples"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

##################################################
# Example Programs
##################################################

set(EXAMPLE_PROGRAMS
    basic_connection
    query_builder_usage
    transaction_management
    error_handling
    orm_entity_demo
    connection_pool_demo
)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

##################################################
# Build Each Example
##################################################

foreach(EXAMPLE ${EXAMPLE_PROGRAMS})
    add_executable(example_${EXAMPLE} ${EXAMPLE}.cpp)

    set_target_properties(example_${EXAMPLE} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_include_directories(example_${EXAMPLE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )

    target_link_libraries(example_${EXAMPLE} PRIVATE
        database
        Threads::Threads
    )

    # Compiler-specific options
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(example_${EXAMPLE} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    elseif(MSVC)
        target_compile_options(example_${EXAMPLE} PRIVATE
            /W4
            /wd4100  # unreferenced formal parameter
        )
    endif()

    # Platform-specific definitions
    if(WIN32)
        target_compile_definitions(example_${EXAMPLE} PRIVATE
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()

    message(STATUS "Database example configured: ${EXAMPLE}")
endforeach()

##################################################
# Installation (Optional)
##################################################

set(EXAMPLE_TARGETS)
foreach(EXAMPLE ${EXAMPLE_PROGRAMS})
    list(APPEND EXAMPLE_TARGETS example_${EXAMPLE})
endforeach()

install(TARGETS ${EXAMPLE_TARGETS}
    RUNTIME DESTINATION bin/examples
    COMPONENT examples
)

# Install example source files for reference
set(EXAMPLE_SOURCES)
foreach(EXAMPLE ${EXAMPLE_PROGRAMS})
    list(APPEND EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE}.cpp)
endforeach()
list(APPEND EXAMPLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
)

install(FILES ${EXAMPLE_SOURCES}
    DESTINATION share/database_system/examples
    COMPONENT examples
)

##################################################
# Summary
##################################################

message(STATUS "Database System Examples configured:")
message(STATUS "  Example programs: ${EXAMPLE_PROGRAMS}")
message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Dependencies: database, Threads")
