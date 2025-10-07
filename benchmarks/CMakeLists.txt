# Database System Benchmarks
# Phase 0, Task 0.2: Baseline Performance Benchmarking

cmake_minimum_required(VERSION 3.20)

find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(BENCHMARK benchmark)
    endif()
endif()

if(benchmark_FOUND OR BENCHMARK_FOUND)
    message(STATUS "Google Benchmark found. Building database_system benchmarks...")

    add_executable(database_benchmarks
        query_execution_bench.cpp
        transaction_bench.cpp
        main_bench.cpp
    )

    target_link_libraries(database_benchmarks
        PRIVATE
            database
            benchmark::benchmark
            benchmark::benchmark_main
    )

    target_compile_features(database_benchmarks PRIVATE cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(database_benchmarks PRIVATE
            -Wall -Wextra -Wpedantic -O3 -DNDEBUG)
    elseif(MSVC)
        target_compile_options(database_benchmarks PRIVATE /W4 /O2 /DNDEBUG)
    endif()

    add_custom_target(run_database_benchmarks
        COMMAND database_benchmarks --benchmark_format=json --benchmark_out=results.json
        COMMAND database_benchmarks
        DEPENDS database_benchmarks
        COMMENT "Running database_system benchmarks"
    )

    install(TARGETS database_benchmarks RUNTIME DESTINATION bin/benchmarks)
else()
    message(WARNING "Google Benchmark not found. Skipping benchmarks.")
endif()
