# Fuzz corpus seeds

Initial corpus for the SQL fuzz harnesses under `fuzz/`. Each file is a raw
byte sequence fed to `LLVMFuzzerTestOneInput`.

The `seed_*` files are classic SQL-injection / escaping edge cases used to
bootstrap coverage for `sql_value_formatter_fuzzer` (single quotes, comment
markers, stacked queries, embedded double-quotes for identifier quoting, and
backslash runs). The same bytes also exercise the byte-stream grammar consumed
by `sql_query_builder_fuzzer`.

Run, for example:

```
cmake -B build-fuzz -S . -DBUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz --target sql_value_formatter_fuzzer
./build-fuzz/fuzz/sql_value_formatter_fuzzer fuzz/corpus
```

libFuzzer will minimize and grow this corpus in place; new interesting inputs
discovered during a run are written back into this directory.
