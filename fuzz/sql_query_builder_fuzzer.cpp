// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

// libFuzzer harness for the SQL query assembly path of database::query_builder.
//
// Rationale:
//   query_builder concatenates table names, column lists, WHERE fields,
//   operators and values into a SQL string via build(). Field names, operators
//   and table names flow into the query with comparatively little structural
//   validation, so this harness fuzzes the builder with attacker-shaped input
//   to surface crashes, unbounded growth or memory-safety issues (caught by
//   ASan) in the assembly logic. The fuzzer derives a small grammar from the
//   input bytes so a single corpus entry maps to a deterministic builder
//   program.
//
// Build (not part of the default build):
//   cmake -B build-fuzz -S . -DBUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
//   cmake --build build-fuzz --target sql_query_builder_fuzzer
//   ./build-fuzz/fuzz/sql_query_builder_fuzzer fuzz/corpus

#include <kcenon/database/database_types.h>
#include <kcenon/database/query_builder.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

// Minimal byte-stream cursor used to deterministically derive a builder program
// from the fuzzer input.
class cursor {
public:
    cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    uint8_t byte() {
        if (pos_ >= size_) {
            return 0;
        }
        return data_[pos_++];
    }

    // Read a length-prefixed token (length is one byte, clamped to 32).
    std::string token() {
        size_t len = byte() % 32u;
        std::string out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            out.push_back(static_cast<char>(byte()));
        }
        return out;
    }

    bool done() const { return pos_ >= size_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

database::database_types pick_db(uint8_t b) {
    return (b % 2u == 0) ? database::database_types::postgres
                         : database::database_types::sqlite;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    cursor cur(data, size);

    database::query_builder builder(pick_db(cur.byte()));

    // Bound the number of operations so a single input cannot run unbounded.
    for (int op = 0; op < 16 && !cur.done(); ++op) {
        switch (cur.byte() % 7u) {
            case 0: {
                std::vector<std::string> cols;
                size_t n = cur.byte() % 5u;
                for (size_t i = 0; i < n; ++i) {
                    cols.push_back(cur.token());
                }
                builder.select(cols);
                break;
            }
            case 1:
                builder.from(cur.token());
                break;
            case 2: {
                std::string field = cur.token();
                std::string oper = cur.token();
                database::core::database_value value = cur.token();
                builder.where(field, oper, value);
                break;
            }
            case 3:
                builder.order_by(cur.token());
                break;
            case 4:
                builder.limit(cur.byte());
                break;
            case 5:
                builder.group_by(cur.token());
                break;
            default:
                builder.insert_into(cur.token());
                break;
        }
    }

    // Drive the assembly path; result is intentionally unused.
    const std::string sql = builder.build();
    (void)sql;

    return 0;
}
