##################################################
# Database System: compiler and language settings
#
# Sets C++ standard, output directories, debug flags, platform-specific
# definitions, optional C++20 coroutine detection, and code coverage
# instrumentation when ENABLE_COVERAGE is ON.
##################################################

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
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

# Coverage Configuration (DB-006)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage -O0 -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")

        # Find coverage tools
        find_program(GCOV_PATH gcov)
        find_program(GCOVR_PATH gcovr)

        if(NOT GCOV_PATH)
            message(WARNING "gcov not found - coverage reports may not work")
        endif()

        if(GCOVR_PATH)
            # Coverage report generation target
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
                COMMAND ${GCOVR_PATH}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude '.*tests/.*'
                    --exclude '.*third_party/.*'
                    --exclude '.*samples/.*'
                    --exclude '.*examples/.*'
                    --exclude '.*benchmarks/.*'
                    --html --html-details
                    --output ${CMAKE_BINARY_DIR}/coverage/index.html
                    --xml ${CMAKE_BINARY_DIR}/coverage/coverage.xml
                    --print-summary
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report..."
            )

            # Coverage threshold check target (80% line coverage)
            add_custom_target(coverage-check
                COMMAND ${GCOVR_PATH}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude '.*tests/.*'
                    --exclude '.*third_party/.*'
                    --exclude '.*samples/.*'
                    --exclude '.*examples/.*'
                    --exclude '.*benchmarks/.*'
                    --fail-under-line 80
                    --print-summary
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Checking coverage threshold (80% line coverage required)..."
            )

            message(STATUS "Coverage targets available: 'coverage', 'coverage-check'")
        else()
            message(WARNING "gcovr not found - install with 'pip install gcovr'")
        endif()
    else()
        message(WARNING "Code coverage only supported with GCC/Clang")
        set(ENABLE_COVERAGE OFF CACHE BOOL "Coverage not supported" FORCE)
    endif()
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
