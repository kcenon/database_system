# Database System 기능

**언어:** [English](FEATURES.md) | **한국어**

**최종 업데이트**: 2025-12-09
**버전**: 3.1

이 문서는 database_system의 모든 기능, 백엔드 구현 및 기능에 대한 포괄적인 세부 정보를 제공합니다.

---

## 목차

- [멀티 백엔드 지원](#멀티-백엔드-지원)
- [ORM 프레임워크](#orm-프레임워크)
- [연결 풀링](#연결-풀링)
- [쿼리 빌더](#쿼리-빌더)
- [탄력적 연결](#탄력적-연결)
- [엔터프라이즈 보안](#엔터프라이즈-보안)
- [성능 모니터링](#성능-모니터링)
- [비동기 작업](#비동기-작업)

---

## 멀티 백엔드 지원

database_system은 일관된 API로 여러 데이터베이스 백엔드에 대한 통합 액세스를 제공합니다.

### PostgreSQL 백엔드

**상태**: ✅ 완전 지원
**구현**: `postgres_manager.h/cpp`

**기능**:
- 고급 쿼리를 통한 JSONB 데이터 타입 지원
- 효율적인 저장을 위한 배열 타입
- 복잡한 쿼리를 위한 CTE (Common Table Expressions)
- 매개변수 바인딩을 통한 준비된 문
- tsvector를 통한 전문 검색
- 동시 인덱스 빌드
- 고급 윈도우 함수
- 구체화된 뷰
- 행 수준 보안

**고급 기능 예시**:
```cpp
// JSONB 작업
auto result = db.create_query_builder(database_types::postgres)
    .select({"id", "data->>'name' as name", "data->'address'->>'city' as city"})
    .from("users")
    .where("data @> '{\"active\": true}'::jsonb")
    .execute(&db);

// 배열 작업
auto array_result = db.create_query_builder(database_types::postgres)
    .insert_into("tags")
    .values({
        {"name", database_value{std::string("product1")}},
        {"tags", database_value{std::string("ARRAY['electronics', 'gadgets', 'new']")}}
    })
    .execute(&db);

// 복잡한 쿼리를 위한 CTE
auto cte_result = db.create_query_builder(database_types::postgres)
    .raw_sql(R"(
        WITH sales_summary AS (
            SELECT
                product_id,
                SUM(amount) as total_sales,
                COUNT(*) as order_count
            FROM orders
            WHERE created_at > NOW() - INTERVAL '30 days'
            GROUP BY product_id
        )
        SELECT p.name, s.total_sales, s.order_count
        FROM products p
        JOIN sales_summary s ON p.id = s.product_id
        ORDER BY s.total_sales DESC
        LIMIT 10
    )")
    .execute(&db);
```

### MySQL 백엔드

**상태**: ✅ 완전 지원
**구현**: `mysql/mysql_manager.h/cpp`

**기능**:
- MATCH AGAINST를 통한 전문 검색
- ACID 준수를 통한 InnoDB 트랜잭션
- 플레이스홀더를 통한 준비된 문
- 저장 프로시저 및 함수
- 트리거 및 이벤트
- 파티셔닝 지원
- 복제 인식
- JSON 컬럼 타입 (MySQL 5.7+)

### SQLite 백엔드

**상태**: ✅ 완전 지원
**구현**: `sqlite/sqlite_manager.h/cpp`

**기능**:
- 동시성을 위한 WAL (Write-Ahead Logging) 모드
- FTS5 전문 검색 엔진
- 테스트를 위한 인메모리 데이터베이스
- 임베디드 데이터베이스 (서버 불필요)
- JSON1 확장 지원
- Common Table Expressions
- 윈도우 함수
- 부분 인덱스
- 생성된 컬럼

### MongoDB 백엔드

**상태**: ✅ 완전 지원
**구현**: `mongodb/mongodb_manager.h/cpp`

**기능**:
- BSON을 통한 문서 기반 저장
- 집계 파이프라인 프레임워크
- 대용량 파일 저장을 위한 GridFS
- 샤딩 및 복제 지원
- 텍스트 검색 인덱스
- 지리공간 쿼리
- 실시간 업데이트를 위한 변경 스트림
- 트랜잭션 (MongoDB 4.0+)
- 시계열 컬렉션 (MongoDB 5.0+)

### Redis 백엔드

**상태**: ✅ 완전 지원
**구현**: `redis/redis_manager.h/cpp`

**기능**:
- 모든 데이터 타입 (문자열, 해시, 리스트, 셋, 정렬된 셋)
- Pub/Sub 메시징 패턴
- MULTI/EXEC를 통한 트랜잭션
- Lua 스크립팅 지원
- 배치 작업을 위한 파이프라이닝
- 영속성 (RDB 및 AOF)
- 클러스터 모드 지원
- 이벤트 소싱을 위한 스트림
- 지리공간 인덱스

---

## ORM 프레임워크

**상태**: ✅ 완전 지원 (C++17 SFINAE 기반)
**구현**: `orm/entity.h`, `orm/entity_manager.h`, `orm/schema_manager.h`

### 엔티티 정의

C++17 SFINAE 기반 매크로를 사용하여 데이터베이스 엔티티 정의:

```cpp
#include <database/orm/entity.h>

class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique() | index("idx_username"))
    ENTITY_FIELD(std::string, email, not_null() | unique())
    ENTITY_FIELD(std::string, password_hash, not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())
    ENTITY_FIELD(std::chrono::system_clock::time_point, updated_at, on_update_now())
    ENTITY_FIELD(bool, is_active, default_value(true))
    ENTITY_FIELD(std::optional<std::string>, profile_image)

    ENTITY_METADATA()
};
```

### 엔티티 작업

**생성 (삽입)**:
```cpp
User user;
user.username = "john_doe";
user.email = "john@example.com";
user.password_hash = hash_password("secure_password");

auto create_result = user.save(db);
if (create_result) {
    std::cout << "사용자 생성됨, ID: " << user.id << std::endl;
}
```

**읽기 (쿼리)**:
```cpp
// ID로 찾기
auto user_result = User::find(db, 12345);
if (user_result) {
    std::cout << "사용자명: " << user_result->username << std::endl;
}

// 조건으로 쿼리
auto active_users = User::query(db)
    .where("is_active = ?", true)
    .where("created_at > ?", one_week_ago)
    .order_by("username")
    .limit(100)
    .execute();

for (const auto& user : active_users) {
    std::cout << user.username << " - " << user.email << std::endl;
}
```

**업데이트**:
```cpp
auto user = User::find(db, 12345);
if (user) {
    user->email = "newemail@example.com";
    user->updated_at = std::chrono::system_clock::now();
    auto update_result = user->save(db);
}

// 대량 업데이트
auto update_count = User::query(db)
    .where("last_login < ?", one_year_ago)
    .update({{"is_active", database_value{false}}});
```

**삭제**:
```cpp
auto user = User::find(db, 12345);
if (user) {
    auto delete_result = user->remove(db);
}

// 대량 삭제
auto delete_count = Post::query(db)
    .where("published_at < ?", two_years_ago)
    .remove();
```

### 관계

**일대다 (One-to-Many)**:
```cpp
class User : public entity_base {
    ENTITY_TABLE("users")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null())

    ENTITY_RELATIONSHIP(has_many, Post, "user_id")
    ENTITY_METADATA()
};

// 관련 게시물 접근
auto user = User::find(db, 12345);
auto posts = user->posts(db);  // 지연 로딩
```

**다대일 (Many-to-One)**:
```cpp
class Post : public entity_base {
    ENTITY_TABLE("posts")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(int64_t, user_id, foreign_key("users", "id"))

    ENTITY_RELATIONSHIP(belongs_to, User, "user_id")
    ENTITY_METADATA()
};

// 관련 사용자 접근
auto post = Post::find(db, 678);
auto author = post->user(db);  // 지연 로딩
```

**다대다 (Many-to-Many)**:
```cpp
class Tag : public entity_base {
    ENTITY_TABLE("tags")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, name, not_null() | unique())

    ENTITY_RELATIONSHIP(many_to_many, Post, "post_tags", "tag_id", "post_id")
    ENTITY_METADATA()
};
```

---

## 연결 풀링

**상태**: ✅ 완전 지원
**구현**: `pool/connection_pool.h/cpp`

### 기본 사용

```cpp
// 연결 풀 구성
auto pool = create_connection_pool(database_types::postgres, {
    .min_connections = 5,
    .max_connections = 50,
    .connection_timeout = std::chrono::seconds(30),
    .idle_timeout = std::chrono::minutes(5),
    .health_check_interval = std::chrono::seconds(60),
    .connection_string = "host=localhost port=5432 dbname=mydb user=admin password=secret"
});

// 풀에서 연결 획득
auto conn = pool->acquire();
if (conn) {
    auto result = conn->execute_query("SELECT * FROM users");
    // 연결은 스코프 종료 시 자동으로 풀에 반환됨
}
```

### 고급 기능

- **동적 크기 조정**: 부하에 따른 자동 확장/축소
- **상태 검사**: 유휴 연결의 주기적 검증
- **연결 폐기**: 수명이 다한 연결 자동 제거
- **모니터링 통합**: 풀 메트릭 수집 및 내보내기

---

## 쿼리 빌더

**상태**: ✅ 완전 지원
**구현**: `query/query_builder.h/cpp`

### 타입 안전 쿼리 빌딩

```cpp
// SELECT 쿼리
auto result = db.create_query_builder(database_types::postgres)
    .select({"id", "username", "email", "created_at"})
    .from("users")
    .where("is_active = ?", true)
    .where("created_at > ?", one_week_ago)
    .order_by("created_at", sort_order::desc)
    .limit(100)
    .offset(0)
    .execute(&db);

// JOIN 쿼리
auto posts_with_authors = db.create_query_builder(database_types::postgres)
    .select({"p.title", "p.content", "u.username as author"})
    .from("posts p")
    .join("users u", "p.user_id = u.id")
    .where("p.published_at > ?", yesterday)
    .order_by("p.published_at", sort_order::desc)
    .execute(&db);

// 집계 쿼리
auto stats = db.create_query_builder(database_types::postgres)
    .select({"COUNT(*) as total", "AVG(amount) as avg_amount", "SUM(amount) as total_amount"})
    .from("orders")
    .where("status = ?", "completed")
    .where("created_at BETWEEN ? AND ?", start_date, end_date)
    .execute(&db);

// INSERT 쿼리
auto insert_result = db.create_query_builder(database_types::postgres)
    .insert_into("users")
    .values({
        {"username", database_value{std::string("new_user")}},
        {"email", database_value{std::string("new@example.com")}},
        {"is_active", database_value{true}}
    })
    .returning({"id"})
    .execute(&db);

// UPDATE 쿼리
auto update_result = db.create_query_builder(database_types::postgres)
    .update("users")
    .set({
        {"email", database_value{std::string("updated@example.com")}},
        {"updated_at", database_value{std::chrono::system_clock::now()}}
    })
    .where("id = ?", user_id)
    .execute(&db);

// DELETE 쿼리
auto delete_result = db.create_query_builder(database_types::postgres)
    .delete_from("sessions")
    .where("expires_at < ?", std::chrono::system_clock::now())
    .execute(&db);
```

---

## 탄력적 연결

**상태**: ✅ 완전 지원
**구현**: `resilience/circuit_breaker.h`, `resilience/retry_policy.h`

### 회로 차단기 패턴

```cpp
// 회로 차단기 구성
circuit_breaker_config config {
    .failure_threshold = 5,           // 5회 실패 후 열림
    .success_threshold = 3,           // 3회 성공 후 닫힘
    .timeout = std::chrono::seconds(30),
    .half_open_max_calls = 3
};

auto breaker = create_circuit_breaker(config);

// 회로 차단기 사용
auto result = breaker->execute([&]() {
    return db.execute_query("SELECT 1");
});

if (result) {
    // 쿼리 성공
} else if (breaker->is_open()) {
    // 회로 열림 - 대체 로직 수행
}
```

### 재시도 정책

```cpp
retry_policy policy {
    .max_retries = 3,
    .initial_delay = std::chrono::milliseconds(100),
    .max_delay = std::chrono::seconds(5),
    .backoff_multiplier = 2.0,
    .jitter_factor = 0.1
};

auto result = execute_with_retry(policy, [&]() {
    return db.execute_query("INSERT INTO logs VALUES (...)");
});
```

---

## 엔터프라이즈 보안

### 감사 로깅

```cpp
audit_logger logger("/var/log/db_audit.log");

db.set_audit_logger(&logger);
db.enable_audit_logging({
    .log_queries = true,
    .log_connections = true,
    .log_schema_changes = true,
    .exclude_tables = {"sessions", "tokens"}
});
```

### 데이터 암호화

```cpp
// 필드 수준 암호화
db.set_encryption_key(encryption_key);
db.enable_field_encryption({
    {"users", {"ssn", "credit_card", "email"}}
});

// TLS 연결
connection_options opts {
    .ssl_mode = ssl_mode::verify_full,
    .ssl_ca_cert = "/path/to/ca.crt",
    .ssl_client_cert = "/path/to/client.crt",
    .ssl_client_key = "/path/to/client.key"
};
```

---

## 성능 모니터링

**상태**: ✅ 완전 지원 (monitoring_system 통합)

### 통합 설정

```cpp
#include <monitoring/metrics_collector.h>

auto metrics = create_metrics_collector();
db.set_metrics_collector(metrics);

// 수집되는 메트릭:
// - 쿼리 실행 시간
// - 연결 풀 사용량
// - 쿼리 오류율
// - 트랜잭션 처리량
```

### 성능 메트릭

| 메트릭 | 설명 | 목표값 |
|--------|------|--------|
| 쿼리 지연시간 (p50) | 중앙값 쿼리 시간 | < 1ms |
| 쿼리 지연시간 (p99) | 99번째 백분위수 | < 10ms |
| 연결 풀 대기 시간 | 연결 획득 시간 | < 100μs |
| 쿼리 처리량 | 초당 쿼리 수 | > 10K |

---

## 비동기 작업

**상태**: ✅ 완전 지원 (thread_system 통합)
**구현**: `async/async_executor.h/cpp`

### 비동기 쿼리 실행

```cpp
#include <database/async/async_executor.h>

async_executor executor(4);  // 4 워커 스레드

// 비동기 쿼리
auto future = executor.submit_query(db, "SELECT * FROM large_table");

// 다른 작업 수행...

// 결과 대기
auto result = future.get();
if (result) {
    for (const auto& row : result.value()) {
        process_row(row);
    }
}
```

### C++20 Concepts 통합

비동기 작업은 이제 컴파일 타임 타입 검증을 위해 C++20 concepts를 활용합니다:

**헤더**: `#include <database/core/concepts.h>`

**사용 가능한 Concepts**:

| Concept | 설명 | 사용 사례 |
|---------|------|----------|
| `SubmittableTask<F, Args...>` | 비동기 executor용 태스크 호출 가능 | `async_executor.submit()` |
| `VoidCallable<F, Args...>` | void를 반환하는 콜백 | 완료 핸들러 |
| `ErrorHandler<F>` | 예외 핸들러 호출 가능 | `on_error()` 콜백 |
| `QueryCallback<F, ResultType>` | 쿼리 결과 핸들러 | `on_query_complete()` |
| `StreamEventHandler<F, EventType>` | 스트림 이벤트 프로세서 | 실시간 데이터 핸들러 |
| `StreamEventFilter<F, EventType>` | 이벤트 필터링 프레디케이트 | 이벤트 필터링 |
| `TransactionAction<F>` | Saga 정방향 액션 | 분산 트랜잭션 |
| `CompensationAction<F>` | Saga 롤백 액션 | 보상 로직 |

**타입 안전 비동기 태스크 제출**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept 제약이 있는 태스크 제출
template<SubmittableTask<database_result> F>
auto submit_query_task(async_executor& executor, F&& func) {
    return executor.submit(std::forward<F>(func));
}

// 사용법 - 컴파일러가 컴파일 타임에 호출 가능 시그니처를 검증
auto future = submit_query_task(executor, [&db]() {
    return db.select_query("SELECT * FROM users");
});
```

**타입 안전 에러 처리**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept 제약이 있는 에러 핸들러 등록
template<ErrorHandler F>
void set_error_handler(F&& handler) {
    error_handler_ = std::forward<F>(handler);
}

// 사용법 - 컴파일러가 예외 핸들러 시그니처를 검증
set_error_handler([](const std::exception& e) {
    log_error("데이터베이스 오류: " + std::string(e.what()));
});
```

**Concepts를 사용한 Saga 패턴**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept 제약이 있는 saga 단계 추가
template<TransactionAction A, CompensationAction C>
void add_saga_step(A&& action, C&& compensation) {
    steps_.emplace_back(
        std::forward<A>(action),
        std::forward<C>(compensation)
    );
}

// 사용법
saga_builder.add_step(
    []() { /* 주문 생성 */ },
    []() { /* 주문 취소 */ }
);
```

**장점**:
- **더 명확한 오류 메시지**: 템플릿 오류가 concept 위반으로 표시됨
- **자체 문서화 코드**: 타입 요구사항이 명시적으로 표현됨
- **더 나은 IDE 지원**: 개선된 자동 완성 및 타입 힌트
- **하위 호환**: 기존 `std::function` 오버로드 유지

### 배치 작업

```cpp
// 비동기 대량 삽입
std::vector<User> users = load_users_from_csv("users.csv");

auto batch_future = executor.submit_batch_insert(db, "users", users, {
    .batch_size = 1000,
    .on_progress = [](size_t completed, size_t total) {
        std::cout << "진행: " << completed << "/" << total << std::endl;
    }
});

auto batch_result = batch_future.get();
std::cout << "삽입됨: " << batch_result.inserted_count << " 행" << std::endl;
```

---

## 성능 특성

### 벤치마크 결과

| 작업 | 처리량 | 지연시간 (p50) | 지연시간 (p99) |
|------|--------|----------------|----------------|
| 단순 SELECT | 50K ops/s | 0.2ms | 1.5ms |
| 인덱스 조회 | 100K ops/s | 0.1ms | 0.5ms |
| INSERT (단일) | 20K ops/s | 0.5ms | 2.0ms |
| 대량 INSERT (배치) | 100K rows/s | - | - |
| 트랜잭션 | 10K tx/s | 1.0ms | 5.0ms |

### 최적화 팁

1. **연결 풀 사용**: 연결 생성 오버헤드 제거
2. **준비된 문 사용**: SQL 파싱 비용 절감
3. **배치 작업 사용**: 네트워크 왕복 최소화
4. **인덱스 최적화**: 쿼리 성능 향상
5. **비동기 작업 활용**: 처리량 극대화

---

**최종 업데이트**: 2025-12-09
**버전**: 3.1

---

Made with ❤️ by 🍀☀🌕🌥 🌊
