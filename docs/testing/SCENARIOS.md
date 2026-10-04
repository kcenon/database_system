# Integration Test Scenario Matrix

Backend-agnostic integration test scenarios covering the cross-backend
contracts that `database_system` promises. Each scenario is implemented as
a GoogleTest `TEST_P` case and runs once per backend in the enabled matrix.

Related files:

- Parameterized suite: `integration_tests/scenarios/parameterized_backend_test.cpp`
- Backend-specific helpers: `integration_tests/framework/backend_fixture.h`
- CI workflow: `.github/workflows/integration.yml`

## Backend Matrix

| Backend    | Availability                | How it runs                                            |
|------------|-----------------------------|--------------------------------------------------------|
| SQLite     | Always (embedded)           | File-based temp DB per test                            |
| PostgreSQL | CI only (or local with env) | Connects to `DATABASE_SYSTEM_IT_PG_URL` when set; otherwise skipped |

SQLite is the mandatory backend; PostgreSQL parameter instances skip at
runtime when the connection URL environment variable is absent, keeping
local development friction low.

## Scenarios

Each row is implemented as one parameterized case (both backends), yielding
two logical test invocations per scenario.

| #  | Scenario                              | Category          | Backends | What it verifies                                            |
|----|---------------------------------------|-------------------|----------|-------------------------------------------------------------|
| 1  | Connect and disconnect                | Connection        | SQLite, PostgreSQL | Establishes a session and tears it down cleanly             |
| 2  | Create, insert, select round-trip     | CRUD              | SQLite, PostgreSQL | Basic DDL + DML + query returns inserted row                |
| 3  | Update returns modified data          | CRUD              | SQLite, PostgreSQL | UPDATE persists and is visible to subsequent SELECT         |
| 4  | Delete removes target rows            | CRUD              | SQLite, PostgreSQL | DELETE removes the row and only that row                    |
| 5  | Bulk insert preserves all rows        | CRUD              | SQLite, PostgreSQL | Batch INSERT retains row count and ordering                 |
| 6  | Transaction commit persists           | Transaction       | SQLite, PostgreSQL | BEGIN + INSERT + COMMIT is durable                          |
| 7  | Transaction rollback discards writes  | Transaction       | SQLite, PostgreSQL | BEGIN + INSERT + ROLLBACK leaves no residue                 |
| 8  | Nested savepoint rollback             | Transaction       | SQLite, PostgreSQL | SAVEPOINT + ROLLBACK TO SAVEPOINT keeps outer writes intact |
| 9  | Read-committed isolation              | Isolation         | SQLite, PostgreSQL | Concurrent txn cannot read another txn's uncommitted writes |
| 10 | Serializable ordering                 | Isolation         | SQLite, PostgreSQL | Interleaved writes serialize correctly or surface conflict  |
| 11 | Concurrent reads                      | Concurrency       | SQLite, PostgreSQL | N threads issuing SELECT converge on the same row count     |
| 12 | Concurrent writes to distinct rows    | Concurrency       | SQLite, PostgreSQL | N threads inserting unique rows all succeed                 |
| 13 | Unique-constraint violation surfaces  | Error handling    | SQLite, PostgreSQL | Duplicate unique value returns an error result              |
| 14 | Syntax error is reported              | Error handling    | SQLite, PostgreSQL | Malformed SQL yields a failure, not a crash                 |
| 15 | Connection loss recovery              | Resilience        | SQLite, PostgreSQL | Reconnect after `disconnect()` restores query capability    |
| 16 | Schema migration adds column          | Schema migration  | SQLite, PostgreSQL | ALTER TABLE ADD COLUMN + query returns new column           |
| 17 | Prepared-statement style parameterize | Query builder     | SQLite, PostgreSQL | Repeated parameterized insert loop matches row expectations |

Total: 17 scenarios x 2 backends = 34 parameterized test cases.
SQLite runs unconditionally; PostgreSQL runs when `DATABASE_SYSTEM_IT_PG_URL`
is set (in CI the GitHub Actions `postgres` service provides it).

## Categories and intent

- **Connection**: exercises the connect/disconnect lifecycle and ensures the
  backend registry can hand out a working session.
- **CRUD**: proves the four basic DML verbs plus table creation for both
  backends using backend-neutral SQL.
- **Transaction**: covers the ACID "A" (atomicity) and "D" (durability)
  boundaries via commit, rollback, and savepoints.
- **Isolation**: checks the backend's default isolation behavior for reads
  across transactions and ordered writes.
- **Concurrency**: exercises the thread-safety contract of the manager and
  the backend driver under parallel reads and disjoint-row writes.
- **Error handling**: asserts that constraint violations and malformed SQL
  return errors rather than undefined behavior or process crashes.
- **Resilience**: forces a disconnect and ensures a subsequent connect
  restores query capability (connection-loss recovery surrogate).
- **Schema migration**: proves that `ALTER TABLE ADD COLUMN` works and is
  visible to subsequent queries.

## How the matrix executes

The suite uses `INSTANTIATE_TEST_SUITE_P` with a backend list assembled at
test-program start:

```cpp
std::vector<BackendKind> EnabledBackends() {
  std::vector<BackendKind> kinds;
#ifdef USE_SQLITE
  kinds.push_back(BackendKind::SQLite);
#endif
  if (std::getenv("DATABASE_SYSTEM_IT_PG_URL") != nullptr) {
    kinds.push_back(BackendKind::PostgreSQL);
  }
  return kinds;
}
```

When a backend is unavailable at fixture setup, the test calls
`GTEST_SKIP()` with an actionable message instead of failing, so the SQLite
path stays green in environments where PostgreSQL is not provisioned.

## Running locally

```bash
# SQLite only (default)
cmake -B build -DDATABASE_BUILD_INTEGRATION_TESTS=ON -DUSE_SQLITE=ON
cmake --build build --target database_integration_tests
./build/bin/database_integration_tests --gtest_filter='BackendParam/*'

# With PostgreSQL (requires a running server; supply your own password)
export DATABASE_SYSTEM_IT_PG_URL="host=localhost port=5432 dbname=${DB} user=${USER} password=${PW}"
./build/bin/database_integration_tests --gtest_filter='BackendParam/*'
```

## CI

The `.github/workflows/integration.yml` workflow spins up a PostgreSQL
service container, exports `DATABASE_SYSTEM_IT_PG_URL`, and runs the
parameterized suite plus the legacy integration tests. PR builds gate on
the matrix passing for both backends.
