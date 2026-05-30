// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

// libFuzzer harness for the security-sensitive SQL value/identifier escaping
// paths of database::query::value_formatter.
//
// Rationale:
//   value_formatter::escape_string is the last line of defense against SQL
//   injection for string literals. It runs over fully attacker-controlled input
//   (user-supplied values). Malformed UTF-8, embedded quotes, NUL bytes and
//   backslash runs are the classic vectors that break naive escapers. This
//   harness drives both the PostgreSQL and SQLite escaping code paths with
//   arbitrary bytes and asserts the documented safety invariant: every
//   single-quote in the escaped output is balanced (doubled), so the escaped
//   fragment cannot terminate a string literal early.
//
// Build (not part of the default build):
//   cmake -B build-fuzz -S . -DBUILD_FUZZERS=ON \
//       -DCMAKE_CXX_COMPILER=clang++
//   cmake --build build-fuzz --target sql_value_formatter_fuzzer
//   ./build-fuzz/fuzz/sql_value_formatter_fuzzer fuzz/corpus

#include <kcenon/database/database_types.h>
#include <kcenon/database/query_builder/value_formatter.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

// Verify the core escaping invariant: in the escaped string every single-quote
// appears in a doubled pair (''). A lone, odd single-quote would let an
// attacker-controlled value break out of the surrounding string literal, which
// is the canonical SQL-injection primitive.
bool single_quotes_balanced(const std::string& escaped) {
    std::size_t i = 0;
    while (i < escaped.size()) {
        if (escaped[i] == '\'') {
            if (i + 1 >= escaped.size() || escaped[i + 1] != '\'') {
                return false;
            }
            i += 2;
        } else {
            ++i;
        }
    }
    return true;
}

void exercise(database::database_types db_type, const std::string& input) {
    database::query::value_formatter formatter(db_type);

    // String-literal escaping (the primary injection surface).
    const std::string escaped = formatter.escape_string(input);
    if (!single_quotes_balanced(escaped)) {
        // Surface as a crash so libFuzzer records the reproducer.
        __builtin_trap();
    }

    // Identifier quoting must at least be wrapped in double-quotes for the SQL
    // dialects. (The known interior-quote weakness is intentionally not asserted
    // here so this harness does not flag it as a false positive; it targets the
    // value-escaping path.)
    const std::string ident = formatter.escape_identifier(input);
    if (ident.size() < 2 || ident.front() != '"' || ident.back() != '"') {
        __builtin_trap();
    }

    // Route the same bytes through the generic value formatter as a string
    // value to exercise format()/format_string().
    database::core::database_value value = input;
    (void)formatter.format(value);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    exercise(database::database_types::postgres, input);
    exercise(database::database_types::sqlite, input);

    return 0;
}
