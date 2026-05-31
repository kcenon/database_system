# ============================================================
# Fuzzing harnesses (libFuzzer)
# ============================================================
#
# These targets are gated behind BUILD_FUZZERS and are NOT part of the default
# build. They require a Clang toolchain with libFuzzer + AddressSanitizer:
#
#   cmake -B build-fuzz -S . -DBUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build-fuzz
#   ./build-fuzz/fuzz/sql_value_formatter_fuzzer fuzz/corpus
#
# The fuzzers link against the main `database` library target and target the
# security-sensitive SQL escaping / query-assembly code paths.

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING
        "BUILD_FUZZERS is ON but the compiler is not Clang; "
        "libFuzzer harnesses require clang++. Skipping fuzz targets.")
    return()
endif()

if(NOT TARGET database)
    message(WARNING
        "BUILD_FUZZERS is ON but the 'database' library target is not defined "
        "(BUILD_DATABASE=OFF?). Skipping fuzz targets.")
    return()
endif()

set(FUZZER_FLAGS -g -O1 -fsanitize=fuzzer,address -fno-omit-frame-pointer)

function(add_sql_fuzzer name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE database)
    target_compile_options(${name} PRIVATE ${FUZZER_FLAGS})
    target_link_options(${name} PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz")
endfunction()

add_sql_fuzzer(sql_value_formatter_fuzzer sql_value_formatter_fuzzer.cpp)
add_sql_fuzzer(sql_query_builder_fuzzer  sql_query_builder_fuzzer.cpp)
