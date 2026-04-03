---
doc_id: "DBS-GUID-015"
doc_title: "Database System 빠른 시작 가이드"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Database System 빠른 시작 가이드

> **Language:** [English](QUICK_START.md) | **한국어**

**버전:** 0.1.0
**최종 업데이트:** 2025-12-14

Database System을 5분 만에 시작하세요.

---

## 사전 요구사항

### 필수 요구사항

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| C++20 컴파일러 | GCC 11+ / Clang 14+ / MSVC 2022+ / Apple Clang 14+ | 예 | C++20 기능 필요 |
| CMake | 3.20+ | 예 | 빌드 시스템 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 및 Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 예 | 스레드 풀 및 비동기 작업 |
| [logger_system](https://github.com/kcenon/logger_system) | latest | 예 | 로깅 인프라 |
| [container_system](https://github.com/kcenon/container_system) | latest | 예 | 데이터 컨테이너 작업 |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | latest | 예 | 성능 모니터링 |

### 데이터베이스 백엔드 (최소 하나 필요)

- PostgreSQL 12+
- SQLite 3.35+
- MongoDB 5.0+
- Redis 6.0+

---

## 설치

### 의존성 클론

```bash
# 모든 의존성 클론
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/container_system.git
git clone https://github.com/kcenon/monitoring_system.git
git clone https://github.com/kcenon/database_system.git

# database_system 빌드
cd database_system
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_POSTGRESQL=ON
cmake --build build
```

### CMake FetchContent 사용

```cmake
include(FetchContent)

FetchContent_Declare(database_system
  GIT_REPOSITORY https://github.com/kcenon/database_system.git
  GIT_TAG v0.1.0  # Pin to a specific release tag; do NOT use main
)
FetchContent_MakeAvailable(database_system)

target_link_libraries(your_target kcenon::database)
```

---

## 첫 번째 프로그램

`main.cpp` 생성:

```cpp
#include "integrated/unified_database_system.h"
#include <iostream>

using namespace database::integrated;

int main() {
    // 로깅 및 모니터링이 포함된 데이터베이스 인스턴스 생성
    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .set_pool_size(2, 10)
        .build();

    if (!db) {
        std::cerr << "데이터베이스 생성 실패\n";
        return 1;
    }

    std::cout << "데이터베이스 초기화 성공!\n";

    // 상태 확인
    auto health = db->check_health();
    std::cout << "상태: " << (health.is_connected ? "연결됨" : "연결 안됨") << "\n";

    return 0;
}
```

---

## 백엔드 선택 예제

```cpp
auto db = unified_database_system::create_builder()
    .set_backend_type(backend_type::postgresql)  // 또는 sqlite
    .enable_logging(db_log_level::info, "./logs")
    .set_pool_size(5, 20)
    .build();

auto result = db->connect("host=localhost dbname=mydb user=admin password=secret");
if (result.is_ok()) {
    std::cout << "연결 성공\n";
}
```

---

## 쿼리 예제

### 기본 쿼리

```cpp
// SELECT 쿼리
auto result = db->execute_query("SELECT * FROM users WHERE id = $1", {1});
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        std::cout << "사용자: " << row["name"].as<std::string>() << "\n";
    }
}

// INSERT 쿼리
auto insert_result = db->execute_query(
    "INSERT INTO users (name, email) VALUES ($1, $2)",
    {"홍길동", "hong@example.com"}
);
```

### 불변 쿼리 빌더 (스레드 안전)

```cpp
#include <database/query/immutable_query_builder.h>

const auto base_query = immutable_query_builder()
    .select({"id", "name", "email"})
    .from("users");

// 분기 1: 활성 사용자
const auto active_users = base_query
    .where("is_active", "=", database_value{true})
    .order_by("name");

// 분기 2: 관리자 사용자 (base_query는 변경되지 않음)
const auto admin_users = base_query
    .where("role", "=", database_value{std::string("admin")})
    .order_by("created_at", sort_order::desc);
```

---

## 다음 단계

- **[빌드 가이드](BUILD_GUIDE.kr.md)** - 상세한 빌드 지침
- **[샘플 가이드](SAMPLES_GUIDE.kr.md)** - 예제 애플리케이션
- **[통합 가이드](INTEGRATION.md)** - 생태계 통합
- **[FAQ](FAQ.md)** - 자주 묻는 질문
- **[문제 해결](TROUBLESHOOTING.md)** - 일반적인 문제

자세한 내용은 영문 버전 [QUICK_START.md](QUICK_START.md)를 참조하세요.

---

**최종 업데이트:** 2025-12-14
