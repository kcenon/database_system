[![CI](https://github.com/kcenon/database_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/database_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml)
[![Doxygen](https://github.com/kcenon/database_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/build-Doxygen.yaml)
[![codecov](https://codecov.io/gh/kcenon/database_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/database_system)
[![License](https://img.shields.io/github/license/kcenon/database_system)](https://github.com/kcenon/database_system/blob/main/LICENSE)

# Database System

> **Language:** [English](README.md) | **한국어**

ORM 프레임워크, 실시간 성능 모니터링, 비동기 연산을 포함한 현대적인 C++20 데이터베이스 추상화 레이어입니다.

## 목차

- [개요](#개요)
- [주요 기능](#주요-기능)
- [요구사항](#요구사항)
- [빠른 시작](#빠른-시작)
- [설치](#설치)
- [아키텍처](#아키텍처)
- [핵심 개념](#핵심-개념)
- [API 개요](#api-개요)
- [예제](#예제)
- [성능](#성능)
- [생태계 통합](#생태계-통합)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

---

## 개요

Database System은 PostgreSQL, SQLite, MongoDB, Redis를 통합된 타입 안전 인터페이스를 통해 지원하는 포괄적인 데이터베이스 솔루션입니다.

**핵심 가치**: 벤더 종속성 제거, 성능 극대화, 통합 타입 안전 인터페이스를 통한 개발 가속화.

> **연결 모드 상태**:
> - **DirectMode**: 프로덕션 준비 완료 (안정)
> - **ProxyMode**: 스텁 구현 (`database_server` 대기 중)

**최신 업데이트**:
- C++20 모듈 지원 추가 (`kcenon.database` 모듈)
- 로컬 연결 풀링 제거 (Phase 4.3) - ProxyMode 마이그레이션
- C++20 Concepts 통합 (SubmittableTask, ErrorHandler, QueryCallback)
- monitoring_system 통합
- 불변 쿼리 빌더 추가

---

## 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| **PostgreSQL 백엔드** | libpqxx 기반 전체 지원 | 안정 |
| **SQLite 백엔드** | 경량 로컬 데이터베이스 | 안정 |
| **MongoDB 백엔드** | NoSQL 문서 저장소 | 실험적 |
| **Redis 백엔드** | 키-값 캐시 저장소 | 실험적 |
| **ORM 프레임워크** | C++20 Concepts 기반 엔티티 매핑 | 안정 |
| **불변 쿼리 빌더** | 스레드 안전 함수형 쿼리 구성 | 안정 |
| **백엔드 레지스트리** | 런타임 백엔드 선택 팩토리 | 안정 |
| **통합 DB 시스템** | 제로 설정 진입점 (빌더 패턴) | 안정 |
| **비동기 연산** | 스레드 풀 기반 비동기 쿼리 | 안정 |
| **C++20 모듈** | 모듈 임포트 지원 | 실험적 |

---

## 요구사항

### 컴파일러 매트릭스

| 컴파일러 | 최소 버전 | 비고 |
|----------|----------|------|
| GCC | 13+ | thread_system 전이 의존성 |
| Clang | 17+ | thread_system 전이 의존성 |
| Apple Clang | 14+ | macOS 지원 |
| MSVC | 2022+ | C++20 기능 필수 |

### 빌드 도구 및 의존성

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| CMake | 3.20+ | 예 | 빌드 시스템 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 및 Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 아니오 | 비동기 연산용 스레드 풀 |
| [container_system](https://github.com/kcenon/container_system) | latest | 아니오 | 데이터 직렬화 |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | latest | 아니오 | 성능 메트릭 |

### 백엔드별 의존성

| 백엔드 | 의존성 | 버전 |
|--------|--------|------|
| PostgreSQL | libpqxx | 7.9.2 |
| SQLite | sqlite3 | 3.45.0+ |
| MongoDB | mongo-cxx-driver | 3.8.0+ |
| Redis | hiredis | 1.2.0+ |

---

## 빠른 시작

```cpp
#include <database/integrated/unified_database_system.h>

int main() {
    // 통합 데이터베이스 시스템 (제로 설정)
    auto db = kcenon::database::unified_database_system::builder()
        .backend("postgresql")
        .connection_string("host=localhost dbname=mydb")
        .build();

    // 쿼리 실행
    auto result = db->execute("SELECT * FROM users WHERE active = true");
    if (result.is_ok()) {
        for (const auto& row : result.value()) {
            // 결과 처리
        }
    }

    return 0;
}
```

---

## 설치

### 의존성과 함께 빌드

```bash
# 생태계 의존성 설치
./scripts/dependency.sh

# 빌드
./scripts/build.sh
```

### CMake 수동 빌드

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_POSTGRESQL=ON
cmake --build build
```

### CMake 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `USE_POSTGRESQL` | ON | PostgreSQL 백엔드 |
| `USE_SQLITE` | OFF | SQLite 백엔드 |
| `USE_MONGODB` | OFF | MongoDB 백엔드 |
| `USE_REDIS` | OFF | Redis 백엔드 |
| `USE_THREAD_SYSTEM` | ON | 스레드 풀 통합 |
| `USE_MONITORING_SYSTEM` | ON | 모니터링 통합 |

---

## 아키텍처

### 모듈 구조

```
database/
  core/           - 백엔드 인터페이스, CRTP 베이스, backend_registry, concepts
  backends/       - postgresql, sqlite, mongodb, redis 구현
  query/          - immutable_query_builder (스레드 안전, 함수형)
  query_builder/  - condition_builder, join_builder, sql_dialect
  orm/            - entity.h (ENTITY_TABLE, ENTITY_FIELD 매크로)
  integrated/     - unified_database_system (제로 설정 진입점)
  async/          - async_operations
  monitoring/     - performance_monitor, pool_metrics
  security/       - secure_connection (TLS/SSL)
```

### 의존성 흐름

```
database_system
+-- common_system (필수)
+-- thread_system (선택)
+-- container_system (선택)
+-- monitoring_system (선택)
```

---

## 핵심 개념

### 불변 쿼리 빌더 (Immutable Query Builder)

스레드 안전한 함수형 스타일의 쿼리 구성:

```cpp
auto query = kcenon::database::immutable_query_builder()
    .select({"name", "email"})
    .from("users")
    .where("active = true")
    .order_by("name")
    .build();
```

### ORM 프레임워크

C++20 Concepts 기반 엔티티 매핑:

```cpp
struct User : kcenon::database::entity_base {
    ENTITY_TABLE("users")
    ENTITY_FIELD(std::string, name, "name", primary_key)
    ENTITY_FIELD(std::string, email, "email", not_null)
    ENTITY_FIELD(bool, active, "active", default_value(true))
};
```

### 백엔드 레지스트리 (Backend Registry)

팩토리 패턴으로 런타임 백엔드 선택 (`#ifdef` 없이):

```cpp
auto& registry = kcenon::database::backend_registry::instance();
auto backend = registry.create("postgresql", config);
```

### 통합 데이터베이스 시스템

빌더 패턴의 제로 설정 진입점으로, 백엔드 설정, 연결, 모니터링을 자동으로 구성합니다.

---

## API 개요

| API | 헤더 | 설명 |
|-----|------|------|
| `database_backend` | `core/database_backend.h` | 순수 가상 백엔드 인터페이스 |
| `backend_registry` | `core/backend_registry.h` | 런타임 백엔드 팩토리 |
| `unified_database_system` | `integrated/unified_database_system.h` | 제로 설정 진입점 |
| `immutable_query_builder` | `query/immutable_query_builder.h` | 스레드 안전 쿼리 빌더 |
| `entity_base` | `orm/entity.h` | ORM 엔티티 기반 |
| `async_operations` | `async/async_operations.h` | 비동기 연산 |

---

## 예제

| 예제 | 난이도 | 설명 |
|------|--------|------|
| basic_query | 초급 | 기본 SQL 쿼리 실행 |
| orm_example | 중급 | ORM 엔티티 CRUD |
| query_builder | 중급 | 불변 쿼리 빌더 사용 |
| multi_backend | 고급 | 다중 백엔드 전환 |
| async_operations | 고급 | 비동기 데이터베이스 연산 |

---

## 성능

### 품질 메트릭

- 모든 CI/CD 파이프라인 정상 (모든 플랫폼)
- ThreadSanitizer / AddressSanitizer 클린
- C++20 Concepts를 활용한 컴파일 타임 타입 검증
- RAII 기반 리소스 관리

### 백엔드별 특성

| 백엔드 | 최적 사용처 | 비고 |
|--------|-----------|------|
| PostgreSQL | 엔터프라이즈 OLTP | 전체 기능 지원 |
| SQLite | 임베디드/로컬 | 경량, 설정 불필요 |
| MongoDB | 문서 저장소 | 실험적 |
| Redis | 캐시/세션 | 실험적 |

---

## 생태계 통합

### 의존성 계층

```
common_system      (Tier 0) [필수]
thread_system      (Tier 1) [선택]
container_system   (Tier 1) [선택]
monitoring_system  (Tier 2) [선택]
database_system    (Tier 3) <-- 이 프로젝트
```

### 통합 프로젝트

| 프로젝트 | 관계 |
|----------|------|
| [common_system](https://github.com/kcenon/common_system) | 필수 의존성 |
| [thread_system](https://github.com/kcenon/thread_system) | 비동기 연산 |
| [container_system](https://github.com/kcenon/container_system) | 데이터 직렬화 |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | 성능 메트릭 |
| [pacs_system](https://github.com/kcenon/pacs_system) | 하위 소비자 |

### 플랫폼 지원

| 플랫폼 | 컴파일러 | 상태 |
|--------|----------|------|
| **Linux** | GCC 13+, Clang 17+ | 완전 지원 |
| **macOS** | Apple Clang 14+ | 완전 지원 |
| **Windows** | MSVC 2022+ | 완전 지원 |

---

## 기여하기

기여를 환영합니다! 자세한 내용은 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 참조하세요.

1. 리포지토리 포크
2. 기능 브랜치 생성
3. 테스트와 함께 변경 사항 작성
4. 로컬에서 테스트 실행
5. Pull Request 열기

---

## 라이선스

이 프로젝트는 BSD 3-Clause 라이선스에 따라 배포됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
