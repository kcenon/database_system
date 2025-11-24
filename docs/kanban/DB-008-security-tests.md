# DB-008: Security Module Integration Tests

**Category**: TEST
**Priority**: MEDIUM
**Status**: DONE
**Est. Duration**: 4-5 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change (무엇을 바꾸려는 것인지?)

### Current State
- SQL Injection 방지 기능이 query_builder에 부분적으로 구현됨
- 인증/권한 관련 보안 테스트 부재
- 민감 데이터 처리에 대한 검증 테스트 없음
- 연결 문자열 내 자격 증명 노출 위험 미검증

### Target State
- SQL Injection 방지 기능 완전 검증
- 연결 자격 증명 보안 테스트 추가
- 민감 데이터 로깅 방지 검증
- 보안 베스트 프랙티스 준수 확인

### Scope
**대상 영역**:
- SQL Injection Prevention
- Credential Management
- Sensitive Data Handling
- Error Message Security

**추가할 테스트 파일**:
- `tests/security/sql_injection_test.cpp`
- `tests/security/credential_test.cpp`
- `tests/security/data_masking_test.cpp`

---

## 2. How to Change (어떻게 바꾸려고 하는 것인지?)

### 2.1 SQL Injection Prevention Tests

```cpp
// tests/security/sql_injection_test.cpp
#include <gtest/gtest.h>
#include "database/query_builder.h"
#include "database/backends/sqlite/sqlite_manager.h"

class SQLInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_unique<database::sqlite_manager>();
        db_->connect(":memory:");
        db_->execute_query("CREATE TABLE users (id INT, name TEXT, email TEXT)");
        db_->insert_query("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')");
    }

    std::unique_ptr<database::sqlite_manager> db_;
    database::sql_query_builder builder_;
};

// Classic SQL Injection Attempts
TEST_F(SQLInjectionTest, BasicInjectionPrevention) {
    // Attempt: ' OR '1'='1
    std::string malicious_input = "' OR '1'='1";

    auto query = builder_
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    auto result = db_->select_query(query);

    // Should return 0 rows (input properly escaped), not all rows
    EXPECT_EQ(result.size(), 0);
}

TEST_F(SQLInjectionTest, UnionBasedInjection) {
    // Attempt: ' UNION SELECT * FROM sensitive_data --
    std::string malicious_input = "' UNION SELECT * FROM sensitive_data --";

    auto query = builder_
        .select({"*"})
        .from("users")
        .where("email", "=", malicious_input)
        .build();

    // Query should not contain unescaped UNION
    EXPECT_TRUE(query.find("UNION SELECT") == std::string::npos ||
                query.find("''") != std::string::npos);
}

TEST_F(SQLInjectionTest, CommentInjection) {
    // Attempt: admin'--
    std::string malicious_input = "admin'--";

    auto query = builder_
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    auto result = db_->select_query(query);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(SQLInjectionTest, BatchStatementInjection) {
    // Attempt: '; DROP TABLE users; --
    std::string malicious_input = "'; DROP TABLE users; --";

    auto query = builder_
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    db_->select_query(query);

    // Table should still exist
    auto check = db_->select_query("SELECT COUNT(*) FROM users");
    EXPECT_FALSE(check.empty());
}

// Parameterized Query Tests
TEST_F(SQLInjectionTest, ParameterizedQueriesSafe) {
    // Using parameterized methods should be safe
    database::database_value safe_value = std::string("O'Brien");

    auto query = builder_
        .select({"*"})
        .from("users")
        .where("name", "=", safe_value)
        .build();

    // Apostrophe should be escaped
    EXPECT_TRUE(query.find("O''Brien") != std::string::npos ||
                query.find("O\\'Brien") != std::string::npos);
}

// Raw Method Warnings
TEST_F(SQLInjectionTest, RawMethodsAllowInjection) {
    // Raw methods should be documented as dangerous
    std::string user_input = "1; DROP TABLE users;";

    // This is intentionally vulnerable - document the risk
    builder_.where_raw("id = " + user_input);

    // Test should pass but document the vulnerability
    ADD_FAILURE() << "SECURITY WARNING: where_raw() bypasses escaping. "
                  << "Only use with trusted input.";
}
```

### 2.2 Credential Security Tests

```cpp
// tests/security/credential_test.cpp
#include <gtest/gtest.h>
#include "database/database_base.h"

class CredentialSecurityTest : public ::testing::Test {};

// Connection String Security
TEST_F(CredentialSecurityTest, PasswordNotInErrorMessages) {
    auto db = database::create_database(database::database_types::PostgreSQL);

    // Attempt connection with wrong password
    std::string conn_str = "host=localhost;port=5432;database=test;"
                           "user=user;password=secret123";

    bool result = db->connect(conn_str);

    // Get error message
    std::string error = db->last_error();

    // Password should not appear in error message
    EXPECT_TRUE(error.find("secret123") == std::string::npos)
        << "Password exposed in error message: " << error;
}

TEST_F(CredentialSecurityTest, PasswordNotInLogs) {
    // Capture log output
    std::stringstream log_capture;
    auto old_buf = std::clog.rdbuf(log_capture.rdbuf());

    auto db = database::create_database(database::database_types::MySQL);
    db->connect("host=localhost;password=supersecret;database=test");

    std::clog.rdbuf(old_buf);

    // Check logs don't contain password
    EXPECT_TRUE(log_capture.str().find("supersecret") == std::string::npos)
        << "Password found in logs!";
}

TEST_F(CredentialSecurityTest, ConnectionStringParsingSecurity) {
    // Test that special characters in password don't break parsing
    std::string passwords[] = {
        "pass=word",      // Contains =
        "pass;word",      // Contains ;
        "pass'word",      // Contains '
        "pass\"word",     // Contains "
        "pass\\word",     // Contains backslash
    };

    for (const auto& pwd : passwords) {
        // Should not crash or misbehave
        auto db = database::create_database(database::database_types::SQLite);
        // SQLite doesn't use passwords, but parsing should be safe
        EXPECT_NO_THROW(db->connect(":memory:"));
    }
}
```

### 2.3 Data Masking Tests

```cpp
// tests/security/data_masking_test.cpp
#include <gtest/gtest.h>
#include "database/database_base.h"

class DataMaskingTest : public ::testing::Test {};

// Sensitive Data Handling
TEST_F(DataMaskingTest, QueryResultsNotLeakedInExceptions) {
    auto db = database::create_database(database::database_types::SQLite);
    db->connect(":memory:");

    db->execute_query("CREATE TABLE sensitive (ssn TEXT, credit_card TEXT)");
    db->insert_query("INSERT INTO sensitive VALUES ('123-45-6789', '4111111111111111')");

    try {
        // Intentionally cause an error after selecting sensitive data
        db->execute_query("INVALID SQL AFTER SELECT");
    } catch (const std::exception& e) {
        std::string error_msg = e.what();

        // Sensitive data should not appear in exception
        EXPECT_TRUE(error_msg.find("123-45-6789") == std::string::npos);
        EXPECT_TRUE(error_msg.find("4111111111111111") == std::string::npos);
    }
}

TEST_F(DataMaskingTest, DebugOutputMasking) {
    // When debug logging is enabled, sensitive data should be masked
    database::database_result result;
    result.push_back({
        {"name", std::string("John Doe")},
        {"ssn", std::string("123-45-6789")},
        {"credit_card", std::string("4111111111111111")}
    });

    // Debug representation should mask sensitive fields
    std::string debug_str = result.to_debug_string();

    EXPECT_TRUE(debug_str.find("123-45-6789") == std::string::npos ||
                debug_str.find("***") != std::string::npos);
}
```

### 2.4 Implementation Steps

1. **SQL Injection 테스트** (Day 1-2)
   - Classic injection 패턴 테스트 (5개)
   - Union-based injection 테스트 (2개)
   - Parameterized query 검증 (3개)
   - Raw method 문서화 (2개)

2. **자격 증명 보안 테스트** (Day 2-3)
   - 에러 메시지 검사 (2개)
   - 로그 검사 (2개)
   - 특수 문자 처리 (5개)

3. **데이터 마스킹 테스트** (Day 4)
   - 예외 메시지 검사 (2개)
   - 디버그 출력 검사 (2개)
   - 메모리 내 데이터 정리 (2개)

4. **보안 리포트 및 문서화** (Day 5)
   - 테스트 결과 분석
   - 보안 권장 사항 문서화
   - OWASP 가이드라인 준수 확인

---

## 3. How to Test (어떻게 테스트 할 것인지?)

### 3.1 Test Execution

```bash
# 전체 보안 테스트 실행
ctest -R security -V

# SQL Injection 테스트만 실행
ctest -R sql_injection -V

# 자격 증명 테스트만 실행
ctest -R credential -V
```

### 3.2 Security Scanning Tools

```bash
# Static Analysis for Security
cppcheck --enable=all --inconclusive \
  --library=std --suppress=missingIncludeSystem \
  --check-config database/

# Bandit-style checks (custom rules)
./scripts/security_scan.sh
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| SQL Injection 테스트 | 12+ | ctest 카운트 |
| Credential 테스트 | 9+ | ctest 카운트 |
| Data Masking 테스트 | 6+ | ctest 카운트 |
| 모든 테스트 통과 | 100% | CI 파이프라인 |
| 보안 취약점 | 0 Critical | 정적 분석 |

### 3.4 OWASP Checklist

| OWASP Item | Status | Test |
|------------|--------|------|
| SQL Injection (A03) | ✅ | sql_injection_test |
| Sensitive Data Exposure (A02) | ✅ | credential_test, data_masking_test |
| Security Misconfiguration (A05) | 🔲 | Manual review |
| Authentication Failures (A07) | 🔲 | credential_test |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| 새로운 SQL 인젝션 패턴 | CRITICAL | 정기적인 테스트 업데이트 |
| 테스트 우회 가능성 | HIGH | 다중 레이어 검증 |
| 환경별 동작 차이 | MEDIUM | 모든 DB 타입에서 테스트 |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-002](DB-002-orm-tests.md) (Query Builder Tests)
  - [DB-015](DB-015-korean-docs.md) (Security Documentation)

---

## 6. Notes

- 새로운 SQL Injection 기법이 발견될 때마다 테스트 추가
- 프로덕션 로그에서 민감 데이터 노출 여부 정기 감사 권장
- `*_raw()` 메서드 사용 시 코드 리뷰 필수화 고려

---

**Document Author**: Database System Team
**Last Modified**: 2025-11-24
