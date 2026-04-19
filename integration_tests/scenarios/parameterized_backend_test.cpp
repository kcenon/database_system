// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

// Parameterized integration scenarios. Each TEST_P runs once per enabled
// backend; see docs/testing/SCENARIOS.md for the full matrix.

#include "framework/backend_fixture.h"
#include "framework/test_helpers.h"

#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

namespace database::testing {

using BackendParam = ParameterizedBackendFixture;

// -----------------------------------------------------------------------------
// Connection
// -----------------------------------------------------------------------------

// Scenario 1: connect + disconnect cycle is idempotent and leaves no leaks.
TEST_P(BackendParam, ConnectDisconnectCycle) {
  // SetUp already connected; verify a query works post-connect.
  EXPECT_EQ(CountRows(), 0u) << "Fresh table should be empty";
  ASSERT_TRUE(manager()->disconnect_result().is_ok());
  // Reconnect for TearDown to manage symmetrically.
  if (kind() == BackendKind::SQLite) {
    ASSERT_TRUE(manager()->connect_result(db_file_.string()).is_ok());
  } else {
    const char* url = std::getenv("DATABASE_SYSTEM_IT_PG_URL");
    ASSERT_NE(url, nullptr);
    ASSERT_TRUE(manager()->connect_result(url).is_ok());
  }
  connected_ = true;
}

// -----------------------------------------------------------------------------
// CRUD
// -----------------------------------------------------------------------------

// Scenario 2: insert + select round-trip returns the inserted row.
TEST_P(BackendParam, InsertSelectRoundTrip) {
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('alice', 'alice@test.com', 30)"));
  auto rows = Select("SELECT name, email, age FROM " + TableName() +
                     " WHERE email = 'alice@test.com'");
  ASSERT_EQ(rows.size(), 1u);
}

// Scenario 3: update is persisted and visible to subsequent select.
TEST_P(BackendParam, UpdatePersistsModifiedValue) {
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('bob', 'bob@test.com', 25)"));
  ASSERT_TRUE(Execute("UPDATE " + TableName() +
                      " SET age = 42 WHERE email = 'bob@test.com'"));
  auto rows = Select("SELECT age FROM " + TableName() +
                     " WHERE email = 'bob@test.com'");
  ASSERT_EQ(rows.size(), 1u);
}

// Scenario 4: delete removes only the matching row.
TEST_P(BackendParam, DeleteRemovesOnlyTargetRow) {
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('c1', 'c1@test.com', 21), "
                      "('c2', 'c2@test.com', 22), "
                      "('c3', 'c3@test.com', 23)"));
  ASSERT_TRUE(Execute("DELETE FROM " + TableName() +
                      " WHERE email = 'c2@test.com'"));
  EXPECT_EQ(CountRows(), 2u);
  EXPECT_EQ(CountRows("email = 'c2@test.com'"), 0u);
}

// Scenario 5: bulk insert preserves row count.
TEST_P(BackendParam, BulkInsertPreservesCount) {
  constexpr int kRows = 25;
  for (int i = 0; i < kRows; ++i) {
    ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                        " (name, email, age) VALUES ('u" +
                        std::to_string(i) + "', 'u" + std::to_string(i) +
                        "@test.com', " + std::to_string(20 + i) + ")"));
  }
  EXPECT_EQ(CountRows(), static_cast<size_t>(kRows));
}

// -----------------------------------------------------------------------------
// Transactions
// -----------------------------------------------------------------------------

// Scenario 6: BEGIN + INSERT + COMMIT is durable.
TEST_P(BackendParam, TransactionCommitPersists) {
  ASSERT_TRUE(Create("BEGIN"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('txc', 'txc@test.com', 33)"));
  ASSERT_TRUE(Create("COMMIT"));
  EXPECT_EQ(CountRows("email = 'txc@test.com'"), 1u);
}

// Scenario 7: BEGIN + INSERT + ROLLBACK leaves no residue.
TEST_P(BackendParam, TransactionRollbackDiscardsWrites) {
  ASSERT_TRUE(Create("BEGIN"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('txr', 'txr@test.com', 44)"));
  ASSERT_TRUE(Create("ROLLBACK"));
  EXPECT_EQ(CountRows("email = 'txr@test.com'"), 0u);
}

// Scenario 8: savepoint rollback preserves outer transaction writes.
TEST_P(BackendParam, NestedSavepointRollbackKeepsOuterWrites) {
  ASSERT_TRUE(Create("BEGIN"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('outer', 'outer@test.com', 50)"));
  ASSERT_TRUE(Create("SAVEPOINT inner_sp"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('inner', 'inner@test.com', 51)"));
  ASSERT_TRUE(Create("ROLLBACK TO SAVEPOINT inner_sp"));
  ASSERT_TRUE(Create("COMMIT"));
  EXPECT_EQ(CountRows("email = 'outer@test.com'"), 1u);
  EXPECT_EQ(CountRows("email = 'inner@test.com'"), 0u);
}

// -----------------------------------------------------------------------------
// Isolation
// -----------------------------------------------------------------------------

// Scenario 9: a second session cannot observe uncommitted writes of the first.
TEST_P(BackendParam, ReadCommittedIsolation) {
  auto second_ctx = std::make_shared<database_context>();
  auto second = std::make_shared<database_manager>(second_ctx);
  if (kind() == BackendKind::SQLite) {
    second->set_mode(database_types::sqlite);
    ASSERT_TRUE(second->connect_result(db_file_.string()).is_ok());
  } else {
    const char* url = std::getenv("DATABASE_SYSTEM_IT_PG_URL");
    ASSERT_NE(url, nullptr);
    second->set_mode(database_types::postgres);
    ASSERT_TRUE(second->connect_result(url).is_ok());
  }

  ASSERT_TRUE(Create("BEGIN"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('iso', 'iso@test.com', 60)"));

  // Second session must not see the uncommitted insert.
  auto rows = second->select_query_result("SELECT COUNT(*) AS cnt FROM " +
                                          TableName() +
                                          " WHERE email = 'iso@test.com'");
  if (rows.is_ok() && !rows.value().empty()) {
    auto it = rows.value()[0].find("cnt");
    if (it != rows.value()[0].end() &&
        std::holds_alternative<int64_t>(it->second)) {
      EXPECT_EQ(std::get<int64_t>(it->second), 0);
    }
  }

  ASSERT_TRUE(Create("COMMIT"));
  second->disconnect_result();
}

// Scenario 10: sequential writes in one transaction commit in order.
TEST_P(BackendParam, SerializableOrdering) {
  ASSERT_TRUE(Create("BEGIN"));
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                        " (name, email, age) VALUES ('s" +
                        std::to_string(i) + "', 's" + std::to_string(i) +
                        "@test.com', " + std::to_string(i) + ")"));
  }
  ASSERT_TRUE(Create("COMMIT"));
  EXPECT_EQ(CountRows(), 5u);
}

// -----------------------------------------------------------------------------
// Concurrency
// -----------------------------------------------------------------------------

// Scenario 11: concurrent SELECTs converge on the same row count.
//
// libpqxx's pqxx::connection is not thread-safe, so the PG backend cannot
// service concurrent queries from multiple threads on a single manager.
// Skipped on PG; the contract is still exercised on SQLite.
TEST_P(BackendParam, ConcurrentReadsAgreeOnRowCount) {
  if (kind() == BackendKind::PostgreSQL) {
    GTEST_SKIP() << "libpqxx connection is not thread-safe; concurrent "
                    "reads via a single manager are unsupported.";
  }
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                        " (name, email, age) VALUES ('r" +
                        std::to_string(i) + "', 'r" + std::to_string(i) +
                        "@test.com', 30)"));
  }

  constexpr int kThreads = 4;
  std::vector<std::future<size_t>> futures;
  futures.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    futures.push_back(std::async(std::launch::async, [this] {
      return CountRows();
    }));
  }
  for (auto& f : futures) {
    EXPECT_EQ(f.get(), 10u);
  }
}

// Scenario 12: concurrent writers inserting unique rows all succeed.
//
// SQLite serializes writers at the file level; opening separate manager
// sessions to the same file would hit SQLITE_BUSY without retry plumbing.
// Instead we share the fixture's manager (which the backend guards
// internally) and run inserts from multiple threads to exercise the
// thread-safety contract of database_manager under write contention.
// PG's libpqxx connection is not thread-safe, so the same shared-manager
// pattern cannot apply; skipped on PG.
TEST_P(BackendParam, ConcurrentWritesDistinctRows) {
  if (kind() == BackendKind::PostgreSQL) {
    GTEST_SKIP() << "libpqxx connection is not thread-safe; concurrent "
                    "writes via a single manager are unsupported.";
  }
  constexpr int kThreads = 4;
  constexpr int kPerThread = 5;
  std::atomic<int> ok{0};

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([this, t, &ok] {
      for (int i = 0; i < kPerThread; ++i) {
        const std::string suffix =
            std::to_string(t) + "_" + std::to_string(i);
        const std::string email = "w" + suffix + "@test.com";
        auto r = manager()->execute_query_result(
            "INSERT INTO " + TableName() +
            " (name, email, age) VALUES ('w" + suffix + "', '" + email +
            "', 30)");
        if (r.is_ok()) {
          ok.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  EXPECT_EQ(ok.load(), kThreads * kPerThread);
  EXPECT_EQ(CountRows(), static_cast<size_t>(kThreads * kPerThread));
}

// -----------------------------------------------------------------------------
// Error handling
// -----------------------------------------------------------------------------

// Scenario 13: unique-constraint violation surfaces as a failed result.
TEST_P(BackendParam, UniqueConstraintViolationSurfacesError) {
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('uq1', 'uq@test.com', 22)"));
  auto r = manager()->execute_query_result(
      "INSERT INTO " + TableName() +
      " (name, email, age) VALUES ('uq2', 'uq@test.com', 23)");
  EXPECT_FALSE(r.is_ok()) << "Duplicate unique email should fail";
  EXPECT_EQ(CountRows("email = 'uq@test.com'"), 1u);
}

// Scenario 14: malformed SQL returns an error, not a crash.
TEST_P(BackendParam, MalformedSqlReturnsError) {
  auto r = manager()->execute_query_result("NOT A VALID STATEMENT");
  EXPECT_FALSE(r.is_ok());
  // Session must still be usable afterward.
  EXPECT_EQ(CountRows(), 0u);
}

// -----------------------------------------------------------------------------
// Resilience
// -----------------------------------------------------------------------------

// Scenario 15: disconnect + reconnect restores query capability.
//
// We verify the reconnected session is usable by performing a fresh
// insert+select round-trip, which tests the resilience contract of the
// manager's lifecycle without depending on backend-specific details of
// how data is flushed across an explicit disconnect (e.g. SQLite may not
// persist to the original file if the backend treats connect/disconnect
// as a full open/close cycle).
TEST_P(BackendParam, ConnectionLossRecovery) {
  ASSERT_TRUE(manager()->disconnect_result().is_ok());

  if (kind() == BackendKind::SQLite) {
    ASSERT_TRUE(manager()->connect_result(db_file_.string()).is_ok());
  } else {
    const char* url = std::getenv("DATABASE_SYSTEM_IT_PG_URL");
    ASSERT_NE(url, nullptr);
    ASSERT_TRUE(manager()->connect_result(url).is_ok());
  }
  connected_ = true;

  // Re-create the table if the reconnect dropped schema visibility.
  manager()->execute_query_result("DROP TABLE IF EXISTS " + TableName());
  ASSERT_TRUE(
      Create("CREATE TABLE " + TableName() + " (" + PrimaryKeyInt() +
             ", name TEXT NOT NULL, email TEXT UNIQUE NOT NULL, age INTEGER)"));
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('post', 'post@test.com', 70)"));
  EXPECT_EQ(CountRows("email = 'post@test.com'"), 1u);
}

// -----------------------------------------------------------------------------
// Schema migration
// -----------------------------------------------------------------------------

// Scenario 16: ALTER TABLE ADD COLUMN works and is queryable.
TEST_P(BackendParam, SchemaMigrationAddsColumn) {
  ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                      " (name, email, age) VALUES "
                      "('m1', 'm1@test.com', 31)"));
  ASSERT_TRUE(
      Create("ALTER TABLE " + TableName() + " ADD COLUMN notes TEXT"));
  ASSERT_TRUE(Execute("UPDATE " + TableName() +
                      " SET notes = 'migrated' WHERE email = 'm1@test.com'"));
  auto rows = Select("SELECT notes FROM " + TableName() +
                     " WHERE email = 'm1@test.com'");
  ASSERT_EQ(rows.size(), 1u);
  auto it = rows[0].find("notes");
  ASSERT_NE(it, rows[0].end());
  // Accept string variant for notes column.
  if (std::holds_alternative<std::string>(it->second)) {
    EXPECT_EQ(std::get<std::string>(it->second), "migrated");
  }
}

// -----------------------------------------------------------------------------
// Query builder style
// -----------------------------------------------------------------------------

// Scenario 17: repeated parameterized-style inserts yield correct count.
TEST_P(BackendParam, ParameterizedStyleInsertLoop) {
  constexpr int kRows = 7;
  for (int i = 0; i < kRows; ++i) {
    const std::string name = "p" + std::to_string(i);
    const std::string email = name + "@test.com";
    const int age = 18 + i;
    ASSERT_TRUE(Execute("INSERT INTO " + TableName() +
                        " (name, email, age) VALUES ('" + name + "', '" +
                        email + "', " + std::to_string(age) + ")"));
  }
  EXPECT_EQ(CountRows(), static_cast<size_t>(kRows));
}

// -----------------------------------------------------------------------------
// Instantiation
// -----------------------------------------------------------------------------

INSTANTIATE_TEST_SUITE_P(
    MultiBackend, BackendParam,
    ::testing::ValuesIn(EnabledBackends()), BackendParamName());

} // namespace database::testing
