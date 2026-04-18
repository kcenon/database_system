// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include "database/core/database_context.h"
#include "database/database_manager.h"
#include "database/database_types.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace database::testing {

/**
 * @enum BackendKind
 * @brief Logical backends targeted by the parameterized integration suite.
 */
enum class BackendKind { SQLite, PostgreSQL };

inline const char* BackendName(BackendKind kind) {
  switch (kind) {
  case BackendKind::SQLite:
    return "SQLite";
  case BackendKind::PostgreSQL:
    return "PostgreSQL";
  }
  return "Unknown";
}

/**
 * @brief Returns the list of backends the current build can exercise.
 *
 * SQLite is included whenever USE_SQLITE is defined (CI default).
 * PostgreSQL is only included when DATABASE_SYSTEM_IT_PG_URL is set in the
 * environment, which the integration workflow provides via its service
 * container.
 */
inline std::vector<BackendKind> EnabledBackends() {
  std::vector<BackendKind> kinds;
#ifdef USE_SQLITE
  kinds.push_back(BackendKind::SQLite);
#endif
  if (const char* pg = std::getenv("DATABASE_SYSTEM_IT_PG_URL");
      pg != nullptr && pg[0] != '\0') {
    kinds.push_back(BackendKind::PostgreSQL);
  }
  return kinds;
}

/**
 * @class ParameterizedBackendFixture
 * @brief Fixture that provisions a connected database_manager per backend.
 *
 * The fixture is parameterized on BackendKind and chooses the connection
 * string + DDL dialect based on the parameter. SQLite uses a temporary
 * file per test. PostgreSQL uses DATABASE_SYSTEM_IT_PG_URL and a unique
 * table-name prefix per test to avoid cross-test interference.
 */
class ParameterizedBackendFixture
    : public ::testing::TestWithParam<BackendKind> {
protected:
  void SetUp() override {
    kind_ = GetParam();

    context_ = std::make_shared<database_context>();
    manager_ = std::make_shared<database_manager>(context_);

    switch (kind_) {
    case BackendKind::SQLite:
      SetUpSQLite();
      break;
    case BackendKind::PostgreSQL:
      SetUpPostgreSQL();
      break;
    }

    if (skipped_) {
      return;
    }

    CreateBaseTable();
  }

  void TearDown() override {
    if (skipped_) {
      return;
    }

    if (connected_) {
      // Best-effort cleanup; ignore errors during teardown.
      manager_->execute_query_result(std::string("DROP TABLE IF EXISTS ") +
                                     TableName());
      manager_->disconnect_result();
      connected_ = false;
    }

    if (kind_ == BackendKind::SQLite && !db_file_.empty()) {
      std::error_code ec;
      std::filesystem::remove(db_file_, ec);
    }
  }

  /** @brief Table used by every scenario; unique per parameter instance. */
  std::string TableName() const { return table_name_; }

  /** @brief Backend-appropriate INTEGER PRIMARY KEY clause. */
  std::string PrimaryKeyInt() const {
    return kind_ == BackendKind::SQLite
               ? "id INTEGER PRIMARY KEY AUTOINCREMENT"
               : "id SERIAL PRIMARY KEY";
  }

  /** @brief Execute a DDL/DML statement, returning true on success. */
  bool Execute(const std::string& sql) {
    return manager_->execute_query_result(sql).is_ok();
  }

  /** @brief Execute DDL via create_query_result. */
  bool Create(const std::string& sql) {
    return manager_->create_query_result(sql).is_ok();
  }

  /** @brief Fetch rows as the standard database_result shape. */
  core::database_result Select(const std::string& sql) {
    auto r = manager_->select_query_result(sql);
    if (r.is_ok()) {
      return r.value();
    }
    return {};
  }

  size_t CountRows(const std::string& extra_where = "") {
    std::string sql = "SELECT COUNT(*) AS cnt FROM " + table_name_;
    if (!extra_where.empty()) {
      sql += " WHERE " + extra_where;
    }
    auto rows = Select(sql);
    if (rows.empty()) {
      return 0;
    }
    auto it = rows[0].find("cnt");
    if (it == rows[0].end()) {
      return 0;
    }
    const auto& v = it->second;
    if (std::holds_alternative<int64_t>(v)) {
      return static_cast<size_t>(std::get<int64_t>(v));
    }
    if (std::holds_alternative<std::string>(v)) {
      try {
        return std::stoul(std::get<std::string>(v));
      } catch (...) {
        return 0;
      }
    }
    if (std::holds_alternative<double>(v)) {
      return static_cast<size_t>(std::get<double>(v));
    }
    return 0;
  }

  /** @brief Creates the common base table used by scenarios. */
  void CreateBaseTable() {
    const std::string create = "CREATE TABLE " + table_name_ + " (" +
                               PrimaryKeyInt() +
                               ", name TEXT NOT NULL"
                               ", email TEXT UNIQUE NOT NULL"
                               ", age INTEGER)";
    // Drop any leftover first (PostgreSQL may reuse a DB across runs).
    manager_->execute_query_result("DROP TABLE IF EXISTS " + table_name_);
    ASSERT_TRUE(Create(create))
        << "Failed to create base table on " << BackendName(kind_);
  }

  BackendKind kind() const { return kind_; }
  database_manager* manager() { return manager_.get(); }

private:
  void SetUpSQLite() {
#ifndef USE_SQLITE
    skipped_ = true;
    GTEST_SKIP() << "SQLite not compiled (USE_SQLITE undefined).";
    return;
#else
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    db_file_ = std::filesystem::temp_directory_path() /
               ("db_it_" + std::to_string(now) + ".db");
    table_name_ = "it_table_" + std::to_string(now);

    manager_->set_mode(database_types::sqlite);
    auto r = manager_->connect_result(db_file_.string());
    connected_ = r.is_ok();
    if (!connected_) {
      skipped_ = true;
      GTEST_SKIP() << "Failed to connect SQLite backend at " << db_file_;
    }
#endif
  }

  void SetUpPostgreSQL() {
    const char* url = std::getenv("DATABASE_SYSTEM_IT_PG_URL");
    if (url == nullptr || url[0] == '\0') {
      skipped_ = true;
      GTEST_SKIP() << "DATABASE_SYSTEM_IT_PG_URL not set; "
                      "PostgreSQL backend unavailable in this environment.";
      return;
    }
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    table_name_ = "it_table_" + std::to_string(now);

    manager_->set_mode(database_types::postgres);
    auto r = manager_->connect_result(url);
    connected_ = r.is_ok();
    if (!connected_) {
      skipped_ = true;
      GTEST_SKIP() << "Failed to connect PostgreSQL backend at: " << url;
    }
  }

protected:
  BackendKind kind_{BackendKind::SQLite};
  std::shared_ptr<database_context> context_;
  std::shared_ptr<database_manager> manager_;
  std::filesystem::path db_file_;
  std::string table_name_;
  bool connected_{false};
  bool skipped_{false};
};

/** @brief gtest PrintTo for readable parameter names. */
inline void PrintTo(const BackendKind& kind, std::ostream* os) {
  *os << BackendName(kind);
}

/** @brief Name generator for INSTANTIATE_TEST_SUITE_P. */
struct BackendParamName {
  std::string operator()(
      const ::testing::TestParamInfo<BackendKind>& info) const {
    return BackendName(info.param);
  }
};

} // namespace database::testing
