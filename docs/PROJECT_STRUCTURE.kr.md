# Database System 프로젝트 구조

**언어:** [English](PROJECT_STRUCTURE.md) | **한국어**

**최종 업데이트**: 2025-11-28
**버전**: 3.0

이 문서는 database_system 디렉토리 구성, 모듈 설명 및 빌드 설정에 대한 포괄적인 가이드를 제공합니다.

---

## 목차

- [디렉토리 개요](#디렉토리-개요)
- [모듈 구성](#모듈-구성)
- [파일 설명](#파일-설명)
- [빌드 시스템](#빌드-시스템)
- [의존성](#의존성)
- [통합 지점](#통합-지점)

---

## 디렉토리 개요

```
database_system/
├── include/database/              # 공개 헤더
│   ├── core/                      # 코어 추상화
│   ├── backends/                  # 데이터베이스 백엔드 구현
│   ├── query/                     # 쿼리 빌딩 및 실행
│   ├── orm/                       # 객체-관계 매핑
│   ├── security/                  # 엔터프라이즈 보안
│   ├── monitoring/                # 성능 모니터링
│   ├── async/                     # 비동기 연산
│   ├── remote/                    # 원격 데이터베이스 접근
│   ├── resilient/                 # 복원력 있는 연결
│   ├── integrated/                # 통합 데이터베이스 시스템
│   └── adapters/                  # 시스템 어댑터
├── src/                           # 구현 파일
│   ├── core/                      # 코어 구현
│   ├── backends/                  # 백엔드 구현
│   ├── query/                     # 쿼리 구현
│   ├── orm/                       # ORM 구현
│   ├── security/                  # 보안 구현
│   ├── monitoring/                # 모니터링 구현
│   ├── async/                     # 비동기 구현
│   ├── remote/                    # 원격 구현
│   ├── resilient/                 # 복원력 구현
│   ├── integrated/                # 통합 시스템 구현
│   └── adapters/                  # 어댑터 구현
├── samples/                       # 예제 프로그램
│   ├── basic_usage/               # 기본 연산
│   ├── postgres_advanced/         # 고급 PostgreSQL
│   ├── connection_pool_demo/      # 커넥션 풀링
│   ├── orm_examples/              # ORM 사용법
│   ├── enterprise_features/       # 보안 & 모니터링
│   ├── query_examples/            # 쿼리 빌더
│   ├── multi_database/            # 멀티 백엔드 사용
│   ├── async_examples/            # 비동기 연산
│   ├── integrated/                # 통합 시스템 예제
│   ├── container_integration/     # Container 시스템 통합
│   ├── messaging_integration/     # 메시징 통합
│   └── monitoring_integration/    # 모니터링 통합
├── tests/                         # 모든 테스트
│   ├── unit/                      # 유닛 테스트
│   │   ├── core/                  # 코어 테스트
│   │   ├── backends/              # 백엔드 테스트
│   │   ├── query/                 # 쿼리 테스트
│   │   ├── orm/                   # ORM 테스트
│   │   ├── security/              # 보안 테스트
│   │   ├── monitoring/            # 모니터링 테스트
│   │   ├── async/                 # 비동기 테스트
│   │   ├── remote/                # 원격 테스트
│   │   ├── resilient/             # 복원력 테스트
│   │   ├── integrated/            # 통합 시스템 테스트
│   │   └── adapters/              # 어댑터 테스트
│   ├── integration/               # 통합 테스트
│   │   ├── postgres/              # PostgreSQL 통합
│   │   ├── mysql/                 # MySQL 통합
│   │   ├── sqlite/                # SQLite 통합
│   │   ├── mongodb/               # MongoDB 통합
│   │   ├── redis/                 # Redis 통합
│   │   └── multi_backend/         # 멀티 백엔드 테스트
│   └── performance/               # 성능 테스트
│       ├── connection_pool/       # 풀 벤치마크
│       ├── query_performance/     # 쿼리 벤치마크
│       ├── concurrent/            # 동시성 테스트
│       └── memory/                # 메모리 프로파일링
├── benchmarks/                    # 성능 벤치마크
│   ├── scripts/                   # 벤치마크 스크립트
│   ├── results/                   # 벤치마크 결과
│   └── BASELINE.md                # 기준선 메트릭
├── docs/                          # 문서
│   ├── README.md                  # 문서 인덱스
│   ├── FEATURES.md                # 상세 기능
│   ├── BENCHMARKS.md              # 성능 벤치마크
│   ├── PROJECT_STRUCTURE.md       # 이 파일
│   ├── PRODUCTION_QUALITY.md      # 프로덕션 품질
│   ├── 01-ARCHITECTURE.md         # 아키텍처 개요
│   ├── 02-API_REFERENCE.md        # API 문서
│   ├── advanced/                  # 고급 가이드
│   ├── guides/                    # 사용자 가이드
│   ├── contributing/              # 기여 가이드
│   ├── integration/               # 통합 가이드
│   └── performance/               # 성능 문서
├── cmake/                         # CMake 모듈
│   ├── FindPostgreSQL.cmake       # PostgreSQL finder
│   ├── FindMySQL.cmake            # MySQL finder
│   ├── FindSQLite3.cmake          # SQLite3 finder
│   ├── FindMongoDB.cmake          # MongoDB finder
│   ├── FindRedis.cmake            # Redis finder
│   └── CompilerWarnings.cmake     # 컴파일러 설정
├── scripts/                       # 빌드 및 유틸리티 스크립트
│   ├── dependency.sh              # 의존성 설치 (Linux/macOS)
│   ├── dependency.bat             # 의존성 설치 (Windows)
│   ├── dependency.ps1             # 의존성 설치 (PowerShell)
│   ├── build.sh                   # 빌드 스크립트 (Linux/macOS)
│   ├── build.bat                  # 빌드 스크립트 (Windows)
│   ├── build.ps1                  # 빌드 스크립트 (PowerShell)
│   └── run_tests.sh               # 테스트 실행기
├── .github/                       # GitHub 설정
│   └── workflows/                 # CI/CD 워크플로우
│       ├── ci.yml                 # 메인 CI 파이프라인
│       ├── coverage.yml           # 커버리지 리포팅
│       ├── static-analysis.yml    # 정적 분석
│       └── build-Doxygen.yaml     # 문서 빌드
├── CMakeLists.txt                 # 메인 CMake 설정
├── vcpkg.json                     # vcpkg 의존성
├── LICENSE                        # BSD 3-Clause 라이선스
└── README.md                      # 메인 README
```

---

## 모듈 구성

### Core 모듈 (`include/database/core/`, `src/core/`)

**목적**: 모든 데이터베이스 연산을 위한 기초 추상화 및 인터페이스

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `database_base.h` | 데이터베이스 백엔드를 위한 추상 인터페이스 | 250 |
| `database_manager.h` | 풀링이 포함된 싱글톤 관리자 | 350 |
| `database_types.h` | 타입 정의 및 열거형 | 180 |
| `connection_pool.h` | 엔터프라이즈 커넥션 풀링 | 450 |
| `connection_wrapper.h` | RAII 커넥션 래퍼 | 120 |
| `database_exceptions.h` | 예외 계층구조 | 80 |

**책임**:
- 추상 `database_base` 인터페이스 정의
- 데이터베이스 백엔드 라이프사이클 관리
- 커넥션 풀링 인프라 제공
- 데이터 타입 정의 (`database_value`, `database_row`, `database_result`)
- 데이터베이스 독립적 연산 처리

### Backend 모듈 (`include/database/backends/`, `src/backends/`)

**목적**: 각 데이터베이스 백엔드에 대한 구체적 구현

#### PostgreSQL 백엔드

**파일**:
- `postgres_manager.h/cpp`: PostgreSQL 구현 (850 LOC)
- `postgres_connection.h/cpp`: 커넥션 처리 (320 LOC)
- `postgres_prepared_statement.h/cpp`: 준비된 문 (280 LOC)

**기능**:
- JSONB 지원
- 배열 타입
- CTEs (Common Table Expressions)
- 준비된 문
- 전문 검색

**의존성**:
- libpqxx (PostgreSQL C++ 클라이언트 라이브러리)
- OpenSSL (TLS/SSL용)

#### MySQL 백엔드

**파일**:
- `mysql/mysql_manager.h/cpp`: MySQL 구현 (780 LOC)
- `mysql/mysql_connection.h/cpp`: 커넥션 처리 (300 LOC)
- `mysql/mysql_prepared_statement.h/cpp`: 준비된 문 (260 LOC)

**기능**:
- 전문 검색 (MATCH AGAINST)
- InnoDB 트랜잭션
- 준비된 문
- 저장 프로시저

#### SQLite 백엔드

**파일**:
- `sqlite/sqlite_manager.h/cpp`: SQLite 구현 (620 LOC)
- `sqlite/sqlite_connection.h/cpp`: 커넥션 처리 (240 LOC)

**기능**:
- WAL 모드
- FTS5 전문 검색
- 인메모리 데이터베이스
- JSON1 확장

#### MongoDB 백엔드

**파일**:
- `mongodb/mongodb_manager.h/cpp`: MongoDB 구현 (920 LOC)
- `mongodb/mongodb_connection.h/cpp`: 커넥션 처리 (380 LOC)
- `mongodb/gridfs_handler.h/cpp`: GridFS 지원 (280 LOC)

**기능**:
- 문서 연산
- 집계 파이프라인
- 대용량 파일을 위한 GridFS
- Change streams

#### Redis 백엔드

**파일**:
- `redis/redis_manager.h/cpp`: Redis 구현 (680 LOC)
- `redis/redis_connection.h/cpp`: 커넥션 처리 (260 LOC)
- `redis/redis_pubsub.h/cpp`: Pub/Sub 지원 (220 LOC)

**기능**:
- 모든 데이터 타입 (String, Hash, List, Set, Sorted Set)
- Pub/Sub 메시징
- 트랜잭션 (MULTI/EXEC)
- Lua 스크립팅
- 파이프라이닝

### Query 모듈 (`include/database/query/`, `src/query/`)

**목적**: SQL 및 NoSQL을 위한 타입 안전 쿼리 구성

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `query_builder.h` | 추상 쿼리 빌더 인터페이스 | 200 |
| `sql_builder.h` | SQL 쿼리 빌더 | 650 |
| `nosql_builder.h` | NoSQL 쿼리 빌더 | 480 |
| `immutable_query_builder.h` | 스레드 안전 불변 빌더 | 550 |
| `prepared_statement.h` | 준비된 문 지원 | 280 |

**기능**:
- 쿼리 구성을 위한 플루언트 API
- 컴파일 타임 타입 안전성
- SQL 및 NoSQL 지원
- 불변 빌더 (스레드 안전)
- 준비된 문 통합

### ORM 모듈 (`include/database/orm/`, `src/orm/`)

**목적**: C++17 SFINAE를 사용한 객체-관계 매핑

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `entity.h` | 엔티티 베이스 클래스 및 매크로 | 480 |
| `entity_manager.h` | 엔티티 라이프사이클 관리 | 520 |
| `schema_manager.h` | 스키마 생성/마이그레이션 | 620 |
| `relationship.h` | 엔티티 관계 | 380 |
| `query_builder_orm.h` | ORM 쿼리 빌더 | 450 |

**기능**:
- C++17 SFINAE 기반 엔티티 정의
- 자동 스키마 생성
- 타입 안전 엔티티 연산 (CRUD)
- 관계 (일대다, 다대일, 다대다)
- 마이그레이션 및 버전 관리

### Security 모듈 (`include/database/security/`, `src/security/`)

**목적**: 엔터프라이즈 보안 기능

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `secure_connection.h` | TLS/SSL 커넥션 관리 | 380 |
| `credential_manager.h` | 보안 자격 증명 저장 | 320 |
| `access_control.h` | 역할 기반 접근 제어 | 450 |
| `audit_logger.h` | 보안 감사 로깅 | 280 |
| `encryption.h` | 암호화 유틸리티 | 220 |

**기능**:
- TLS/SSL 암호화
- 보안 자격 증명 해싱 (bcrypt, argon2)
- 역할 기반 접근 제어 (RBAC)
- 감사 로깅
- 인증서 검증

### Monitoring 모듈 (`include/database/monitoring/`, `src/monitoring/`)

**목적**: 성능 모니터링 및 관측성

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `performance_monitor.h` | 실시간 메트릭 | 420 |
| `health_monitor.h` | 데이터베이스 헬스 체크 | 320 |
| `prometheus_exporter.h` | Prometheus 통합 | 380 |
| `alert_manager.h` | 성능 알림 | 280 |

**기능**:
- 실시간 성능 메트릭
- 쿼리 지연시간 추적 (P50, P95, P99)
- 커넥션 풀 모니터링
- Prometheus 내보내기
- 알림 임계값

### Async 모듈 (`include/database/async/`, `src/async/`)

**목적**: 비동기 데이터베이스 연산

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `async_operations.h` | C++20 코루틴 지원 | 520 |
| `future_operations.h` | C++17 future 기반 비동기 | 380 |
| `transaction_coordinator.h` | 분산 트랜잭션 | 620 |
| `stream_processor.h` | 실시간 스트리밍 | 450 |

**기능**:
- C++20 코루틴 (선택적)
- C++17 std::future 폴백
- 분산 트랜잭션
- 실시간 데이터 스트리밍
- 비동기 커넥션 풀링

### Integrated 모듈 (`include/database/integrated/`, `src/integrated/`)

**목적**: 제로 설정 통합 데이터베이스 시스템

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `unified_database_system.h` | 통합 인터페이스 | 680 |
| `database_logger_adapter.h` | 로거 어댑터 | 280 |
| `database_monitor_adapter.h` | 모니터 어댑터 | 320 |
| `database_thread_adapter.h` | 스레드 풀 어댑터 | 350 |

**기능**:
- 제로 설정 사용
- 통합 로깅 (logger_system)
- 통합 모니터링 (monitoring_system)
- 통합 스레딩 (thread_system)
- 빌더 패턴 설정
- 폴백 구현

### Adapters 모듈 (`include/database/adapters/`, `src/adapters/`)

**목적**: Result<T> 패턴을 사용한 에코시스템 통합

**주요 파일**:

| 파일 | 설명 | 라인 수 |
|------|-------------|---------------|
| `common_system_adapter.h` | Result<T> 어댑터 | 520 |
| `common_connection_pool_adapter.h` | 풀 어댑터 | 380 |
| `common_database_factory.h` | 팩토리 패턴 | 280 |

**기능**:
- Result<T> 오류 처리
- Result<T>를 사용한 트랜잭션 지원
- 커넥션 풀 통합
- 데이터베이스 생성을 위한 팩토리 패턴

---

## 파일 설명

### 코어 파일

#### `database_base.h/cpp`

**목적**: 모든 데이터베이스 백엔드를 위한 추상 인터페이스

**주요 메서드**:
```cpp
class database_base {
public:
    virtual ~database_base() = default;

    // 커넥션 관리
    virtual bool connect(const std::string& connection_string) = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() const = 0;

    // 쿼리 실행
    virtual database_result select_query(const std::string& query) = 0;
    virtual unsigned int insert_query(const std::string& query) = 0;
    virtual unsigned int update_query(const std::string& query) = 0;
    virtual unsigned int delete_query(const std::string& query) = 0;

    // 트랜잭션 지원
    virtual bool begin_transaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    // 준비된 문
    virtual std::shared_ptr<prepared_statement> prepare(const std::string& query) = 0;
};
```

**상속 계층구조**:
```
database_base (추상)
├── postgres_manager
├── mysql_manager
├── sqlite_manager
├── mongodb_manager
└── redis_manager
```

#### `connection_pool.h/cpp`

**목적**: 엔터프라이즈급 커넥션 풀링

**주요 클래스**:
```cpp
struct connection_pool_config {
    size_t min_connections = 2;
    size_t max_connections = 20;
    std::chrono::milliseconds acquire_timeout{5000};
    std::chrono::milliseconds idle_timeout{30000};
    std::chrono::milliseconds health_check_interval{60000};
    bool enable_health_checks = true;
    std::string connection_string;
};

class connection_pool {
public:
    // 커넥션 획득 (RAII)
    std::shared_ptr<connection_wrapper> acquire_connection(
        connection_priority priority = connection_priority::normal
    );

    // 풀 관리
    void resize(size_t new_size);
    void shutdown();

    // 통계
    pool_statistics get_statistics() const;
    bool is_healthy() const;
};
```

---

## 빌드 시스템

### CMake 설정

**메인 `CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(database_system VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 옵션
option(USE_POSTGRESQL "PostgreSQL 지원 활성화" ON)
option(USE_MYSQL "MySQL 지원 활성화" OFF)
option(USE_SQLITE "SQLite 지원 활성화" OFF)
option(USE_MONGODB "MongoDB 지원 활성화" OFF)
option(USE_REDIS "Redis 지원 활성화" OFF)
option(BUILD_DATABASE_SAMPLES "샘플 프로그램 빌드" ON)
option(USE_UNIT_TEST "유닛 테스트 빌드" ON)
option(BUILD_WITH_COMMON_SYSTEM "common_system 통합 빌드" OFF)

# 의존성 찾기
if(USE_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
endif()
# ... 다른 백엔드들
```

### 빌드 타겟

| 타겟 | 설명 | 명령 |
|--------|-------------|---------|
| `database_system` | 메인 라이브러리 | `cmake --build build --target database_system` |
| `basic_usage` | 기본 예제 | `cmake --build build --target basic_usage` |
| `postgres_advanced` | PostgreSQL 예제 | `cmake --build build --target postgres_advanced` |
| `connection_pool_demo` | 풀 데모 | `cmake --build build --target connection_pool_demo` |
| `database_test` | 모든 유닛 테스트 | `cmake --build build --target database_test` |
| `docs` | Doxygen 문서 | `cmake --build build --target docs` |

---

## 의존성

### 필수 의존성

| 의존성 | 버전 | 목적 |
|------------|---------|---------|
| CMake | 3.16+ | 빌드 시스템 |
| C++ 컴파일러 | C++17 | GCC 7+, Clang 5+, MSVC 2017+ |

### 선택적 데이터베이스 의존성

| 데이터베이스 | 라이브러리 | 버전 | vcpkg 패키지 |
|----------|---------|---------|---------------|
| PostgreSQL | libpqxx | 7.7+ | `libpqxx` |
| MySQL | libmariadb | 3.x+ | `libmariadb` |
| SQLite | sqlite3 | 3.40+ | `sqlite3` |
| MongoDB | mongo-cxx-driver | 3.7+ | `mongo-cxx-driver` |
| Redis | hiredis | 1.1+ | `hiredis` |

### 선택적 시스템 의존성

| 시스템 | 목적 | vcpkg 패키지 |
|--------|---------|---------------|
| thread_system | 커넥션 풀 v3, 비동기 | `kcenon-thread-system` |
| logger_system | 구조화된 로깅 | `kcenon-logger-system` |
| monitoring_system | 향상된 메트릭 | `kcenon-monitoring-system` |
| common_system | Result<T> 패턴 | `kcenon-common-system` |
| container_system | 직렬화 | `kcenon-container-system` |

### 설치

**vcpkg 사용**:
```bash
# 데이터베이스 라이브러리 설치
vcpkg install libpqxx openssl libmariadb sqlite3 mongo-cxx-driver hiredis

# 선택적 시스템 설치 (가능한 경우)
vcpkg install kcenon-common-system kcenon-thread-system
```

**빌드 스크립트 사용**:
```bash
# Linux/macOS
./scripts/dependency.sh

# Windows (명령 프롬프트)
scripts\dependency.bat

# Windows (PowerShell)
.\scripts\dependency.ps1
```

---

## 통합 지점

### Common System 통합

**Result<T> 오류 처리**:

```cpp
#include <database/adapters/common_system_adapter.h>

auto db = std::make_shared<postgres_manager>();
auto adapter = std::make_shared<common_system_database_adapter>(db);

auto result = adapter->connect("host=localhost dbname=mydb");
if (!result) {
    std::cerr << "오류: " << result.error().message << std::endl;
    return -1;
}
```

### Thread System 통합

**스레드 풀이 포함된 커넥션 풀 v3**:

```cpp
#include <thread_system/thread_pool.h>
#include <database/connection_pool.h>

auto thread_pool = std::make_shared<thread_system::thread_pool>(8);

connection_pool_config config;
config.thread_pool = thread_pool;  // thread_system 통합 활성화
config.min_connections = 10;
config.max_connections = 100;

db.create_connection_pool(database_types::postgres, config);
```

### Logger System 통합

**구조화된 로깅**:

```cpp
#include <logger_system/logger.h>
#include <database/integrated/unified_database_system.h>

auto logger = logger_system::createLogger("database.log");

auto db = unified_database_system::builder()
    .with_logger(logger)
    .build();

// 모든 데이터베이스 연산이 자동으로 로깅됨
```

### Monitoring System 통합

**성능 메트릭**:

```cpp
#include <monitoring_system/prometheus_exporter.h>
#include <database/monitoring/performance_monitor.h>

auto& monitor = performance_monitor::instance();
monitor.set_monitoring_system(monitoring_system::instance());

// 메트릭이 자동으로 포트 9090에서 Prometheus로 내보내짐
```

---

## 빌드 아티팩트

### 라이브러리 출력

| 설정 | 출력 | 위치 |
|--------------|--------|----------|
| 정적 라이브러리 | `libdatabase_system.a` | `build/lib/` |
| 공유 라이브러리 | `libdatabase_system.so` | `build/lib/` |
| Windows DLL | `database_system.dll` | `build/bin/` |

### 샘플 프로그램

| 프로그램 | 바이너리 | 위치 |
|---------|--------|----------|
| 기본 사용법 | `basic_usage` | `build/bin/samples/` |
| PostgreSQL 고급 | `postgres_advanced` | `build/bin/samples/` |
| 커넥션 풀 데모 | `connection_pool_demo` | `build/bin/samples/` |
| ORM 예제 | `orm_examples` | `build/bin/samples/` |
| 통합 시스템 | `unified_basic_usage` | `build/bin/samples/integrated/` |

---

## 참고 문서

상세 기능 문서는 [FEATURES.md](FEATURES.md) / [FEATURES.kr.md](FEATURES.kr.md) 참조
성능 벤치마크는 [BENCHMARKS.md](BENCHMARKS.md) / [BENCHMARKS.kr.md](BENCHMARKS.kr.md) 참조
프로덕션 품질 세부사항은 [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md) / [PRODUCTION_QUALITY.kr.md](PRODUCTION_QUALITY.kr.md) 참조

---

**최종 업데이트**: 2025-11-28
**관리자**: kcenon@naver.com

---

Made with ❤️ by 🍀☀🌕🌥 🌊
