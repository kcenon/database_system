---
doc_id: "DBS-FEAT-001"
doc_title: "Database System 기능"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "FEAT"
---

# Database System 기능

**언어:** [English](FEATURES.md) | **한국어**

**최종 업데이트**: 2026-02-08
**버전**: 0.4.0.0

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
- [프록시 모드](#프록시-모드)
- [통합 데이터베이스 시스템](#통합-데이터베이스-시스템)
- [common_system 통합](#common_system-통합)
- [C++20 모듈](#c20-모듈)

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

**상태**: 🧪 실험적 (기본 비활성화)
**구현**: `mongodb/mongodb_manager.h/cpp`

> ⚠️ **실험적**: MongoDB 지원은 기능적이지만 실험적입니다. CMake 옵션 `USE_MONGODB=ON`으로 활성화하세요.

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

**상태**: 🧪 실험적 (기본 비활성화)
**구현**: `redis/redis_manager.h/cpp`

> ⚠️ **실험적**: Redis 지원은 기능적이지만 실험적입니다. CMake 옵션 `USE_REDIS=ON`으로 활성화하세요.

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

### 스키마 관리

**자동 스키마 생성**:
```cpp
#include <database/orm/schema_manager.h>

auto& schema_mgr = schema_manager::instance();

// CREATE TABLE 문 생성
auto create_sql = schema_mgr.generate_schema<User>();

// 모든 테이블 생성
schema_mgr.create_tables(db, {
    schema_mgr.generate_schema<User>(),
    schema_mgr.generate_schema<Post>(),
    schema_mgr.generate_schema<Tag>()
});
```

**스키마 마이그레이션**:
```cpp
// 마이그레이션 정의
migration migration_001("add_user_bio", [](database_manager& db) {
    return db.execute_command("ALTER TABLE users ADD COLUMN bio TEXT");
});

// 마이그레이션 등록 및 실행
schema_mgr.register_migration(migration_001);
schema_mgr.run_migrations(db);

// 마이그레이션 이력 추적
auto applied_migrations = schema_mgr.get_applied_migrations(db);
```

### 고급 ORM 기능

**즉시 로딩 (N+1 쿼리 방지)**:
```cpp
auto users_with_posts = User::query(db)
    .with("posts")              // 게시물 즉시 로딩
    .with("posts.comments")     // 게시물의 댓글 즉시 로딩
    .execute();

// 최적화된 쿼리로 모든 데이터 로드 (N+1 대신 3개 쿼리)
for (const auto& user : users_with_posts) {
    std::cout << user.username << " has " << user.posts().size() << " posts" << std::endl;
}
```

**스코프 (재사용 가능한 쿼리 필터)**:
```cpp
class Post : public entity_base {
    ENTITY_SCOPE(published, [](query_builder& q) {
        return q.where("published_at IS NOT NULL")
                .where("published_at <= ?", std::chrono::system_clock::now());
    })

    ENTITY_SCOPE(popular, [](query_builder& q, int min_views = 1000) {
        return q.where("view_count >= ?", min_views);
    })

    ENTITY_METADATA()
};

// 스코프 사용
auto popular_published = Post::query(db)
    .published()
    .popular(5000)
    .order_by("view_count", sort_order::desc)
    .execute();
```

**소프트 삭제**:
```cpp
class User : public entity_base {
    ENTITY_FIELD(std::optional<std::chrono::system_clock::time_point>, deleted_at)
    ENTITY_SOFT_DELETE("deleted_at")
    ENTITY_METADATA()
};

user->remove(db);                              // deleted_at = NOW() 설정
auto active = User::query(db).execute();       // 자동으로 WHERE deleted_at IS NULL 필터
auto all = User::query(db).with_trashed().execute();  // 삭제된 항목 포함
user->restore(db);                             // deleted_at = NULL 설정
```

**옵저버 (라이프사이클 훅)**:
```cpp
class User : public entity_base {
    ENTITY_OBSERVER(before_create, [](User& user) {
        user.created_at = std::chrono::system_clock::now();
    })

    ENTITY_OBSERVER(before_update, [](User& user) {
        user.updated_at = std::chrono::system_clock::now();
    })

    ENTITY_OBSERVER(after_delete, [](const User& user) {
        audit_log("User deleted: " + user.username);
    })

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

### 우선순위 기반 획득

```cpp
// 중요 작업을 위한 높은 우선순위 연결
auto critical_conn = pool->acquire_connection(connection_priority::high);

// 일반 우선순위 (기본값)
auto normal_conn = pool->acquire_connection(connection_priority::normal);

// 백그라운드 작업을 위한 낮은 우선순위
auto background_conn = pool->acquire_connection(connection_priority::low);
```

### 상태 모니터링

```cpp
// 풀 통계 조회
auto stats = pool->get_statistics();
std::cout << "활성 연결: " << stats.active_connections << std::endl;
std::cout << "사용 가능: " << stats.available_connections << std::endl;
std::cout << "평균 획득 시간: " << stats.avg_acquisition_time.count() << "ns" << std::endl;

// 풀 상태 검사
if (pool->is_healthy()) {
    std::cout << "풀 상태 정상" << std::endl;
}
```

### 정상 종료

```cpp
// 취소 토큰을 사용한 정상 종료
auto shutdown_token = std::make_shared<cancellation_token>();
pool->set_cancellation_token(shutdown_token);

// 종료 시: 활성 연결 대기, 새 요청 거부
pool->shutdown();
```

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

### 불변 쿼리 빌더

스레드 안전한 함수형 프로그래밍 스타일의 쿼리 구성:

```cpp
#include <database/query/immutable_query_builder.h>

// 불변 빌더 (각 메서드가 새 인스턴스를 반환)
const auto base_query = immutable_query_builder()
    .select({"id", "name", "email"})
    .from("users");

// 분기 1: 활성 사용자 (base_query는 변경되지 않음)
const auto active_users = base_query
    .where("is_active", "=", database_value{true})
    .order_by("name");

// 분기 2: 관리자 (base_query는 변경되지 않음)
const auto admin_users = base_query
    .where("role", "=", database_value{std::string("admin")})
    .order_by("created_at", sort_order::desc);

// 스레드 안전: 레이스 컨디션 없음
std::thread t1([&]() { auto r1 = active_users.execute(&db); });
std::thread t2([&]() { auto r2 = admin_users.execute(&db); });
t1.join(); t2.join();
```

### NoSQL 쿼리 빌더

**MongoDB 쿼리 빌더**:
```cpp
#include <database/query/nosql_builder.h>

// 문서 검색
auto result = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .find({
        {"age", {{"$gt", database_value{int64_t(18)}}}},
        {"status", database_value{std::string("active")}}
    })
    .sort("created_at", -1)
    .limit(100)
    .execute(&db);

// 집계 파이프라인
auto agg_query = db.create_query_builder(database_types::mongodb)
    .collection("orders")
    .aggregate({
        {"$match", {{"status", database_value{std::string("completed")}}}},
        {"$group", {
            {"_id", database_value{std::string("$customer_id")}},
            {"total", {{"$sum", database_value{std::string("$amount")}}}}
        }}
    });
```

**Redis 쿼리 빌더**:
```cpp
// 해시 작업
auto redis_hset = db.create_query_builder(database_types::redis)
    .hset("user:1000", {
        {"username", "john_doe"},
        {"email", "john@example.com"}
    })
    .execute(&db);

// 정렬된 셋 작업
auto redis_zadd = db.create_query_builder(database_types::redis)
    .zadd("leaderboard", {
        {1000, "player1"},
        {950, "player2"},
        {1200, "player3"}
    })
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

**구현**: `security/secure_connection.h`

보안 모듈은 `database_context`를 통한 의존성 주입으로 접근 가능한 6개의 전용 컴포넌트를 제공합니다.

### 자격 증명 관리자

암호화된 자격 증명 저장 및 마스터 키 관리:

```cpp
auto context = std::make_shared<database_context>();
auto cred_mgr = context->get_credential_manager();

// 암호화된 자격 증명 저장
security::security_credentials creds;
creds.username = "admin";
creds.password_hash = cred_mgr->hash_password("secure_pass");
creds.auth_method = security::authentication_method::password;
cred_mgr->store_credentials("primary_db", creds);

// 키 순환
cred_mgr->rotate_encryption_keys();
```

**지원 인증 방법**: Password, Certificate, Kerberos, OAuth2, JWT

### 연결 보안

TLS/SSL 및 상호 인증을 통한 보안 연결:

```cpp
security::connection_security conn_sec(creds);
conn_sec.configure_tls("client.crt", "client.key", "ca.crt");
conn_sec.set_cipher_suite("TLS_AES_256_GCM_SHA384");
conn_sec.establish_secure_connection("db.example.com", 5432);
```

### 쿼리 보안

SQL 인젝션 방지 및 쿼리 분석:

```cpp
bool safe = security::query_security::is_query_safe(user_input);
std::string sanitized = security::query_security::sanitize_input(user_input);
bool suspicious = security::query_security::detect_suspicious_patterns(query);
```

### 역할 기반 접근 제어 (RBAC)

세분화된 권한 관리:

```cpp
auto access_ctrl = context->get_access_control();

// 역할 생성 및 할당
security::access_control::role admin_role;
admin_role.name = "db_admin";
admin_role.permissions = {
    security::access_control::permission::select,
    security::access_control::permission::insert,
    security::access_control::permission::admin
};
access_ctrl->create_role(admin_role);
access_ctrl->assign_role_to_user("user_123", "db_admin");

// 권한 확인
bool can_delete = access_ctrl->check_permission("user_123", "users", "DELETE");

// 세션 관리
auto session_id = access_ctrl->create_session("user_123", "192.168.1.100");
access_ctrl->cleanup_expired_sessions();
```

### 감사 로깅

보안 이벤트 로깅 및 보고:

```cpp
auto audit_log = context->get_audit_logger();

audit_log->log_database_access("user_123", session_id, "SELECT", "users", query_hash, true);
audit_log->log_authentication_event("user_123", "192.168.1.100", true, "password");

// 보안 보고서 생성
auto report = audit_log->generate_security_report(std::chrono::hours(720));
auto suspicious = audit_log->detect_suspicious_activity(std::chrono::hours(24));

// 로그 내보내기
audit_log->export_logs_to_file("audit_2026_Q1.log");
```

### 보안 모니터

실시간 위협 감지 및 알림:

```cpp
auto sec_monitor = context->get_security_monitor();

// 알림 핸들러 등록
sec_monitor->register_security_handler([](const auto& alert) {
    if (alert.level == security::security_monitor::threat_level::critical) {
        send_alert_notification(alert.description);
    }
});

// 보안 메트릭
auto failed_logins = sec_monitor->get_failed_login_count(std::chrono::hours(1));
double security_score = sec_monitor->calculate_security_score();
```

### 암호화 관리자

필드 수준 및 컬럼 수준 데이터 암호화:

```cpp
auto enc_mgr = context->get_encryption_manager();

// 마스터 키 설정
enc_mgr->set_master_encryption_key("master-encryption-key-256bit");

// 컬럼 수준 암호화 구성
enc_mgr->configure_encrypted_column("users", "ssn", security::encryption_type::aes256);
enc_mgr->configure_encrypted_column("users", "credit_card", security::encryption_type::aes256);

// 필드 데이터 암호화/복호화
auto encrypted_ssn = enc_mgr->encrypt_field_data("123-45-6789", "ssn");
auto decrypted_ssn = enc_mgr->decrypt_field_data(encrypted_ssn, "ssn");

// 키 순환
enc_mgr->rotate_field_key("ssn");
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

## 프록시 모드

**상태**: ✅ 완전 지원 (Phase 4.1)
**구현**: `proxy/proxy_config.h`, `proxy/proxy_connector.h`

프록시 모드는 데이터베이스에 직접 연결하는 대신 database_server 미들웨어를 통해 연결할 수 있게 합니다. 중앙화된 연결 관리, 보안 적용 및 로드 밸런싱을 지원합니다.

```cpp
#include <database/proxy/proxy_config.h>

database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;
config.auth_token = "client-token-xyz";
config.connection_timeout = std::chrono::milliseconds{5000};
config.query_timeout = std::chrono::milliseconds{30000};
config.retry_count = 3;
config.use_tls = true;

// mTLS (상호 TLS) 지원
config.client_cert_path = "/etc/ssl/client.crt";
config.client_key_path = "/etc/ssl/client.key";

// 프록시 모드 설정
manager->set_connection_mode(connection_mode::proxy);
manager->configure_proxy(config);
```

---

## 통합 데이터베이스 시스템

**상태**: ✅ 완전 지원 (Phase 6)
**구현**: `integrated/unified_database_system.h`

모든 어댑터(로거, 모니터링, 스레드)를 통합한 제로 구성 진입점:

```cpp
using namespace database::integrated;

// 1. 제로 구성 사용 (가장 간단)
unified_database_system db;
auto result = db.connect("postgresql://localhost/mydb");

// 2. 빌더 패턴 구성
auto db = unified_database_system::builder()
    .set_backend(backend_type::postgresql)
    .set_connection_string("host=localhost dbname=mydb")
    .set_pool_size(10, 50)
    .enable_logging(db_log_level::debug, "./logs")
    .enable_monitoring(true)
    .enable_async(4)  // 4 워커 스레드
    .build();

// 3. 비동기 쿼리 실행
auto future = db->execute_async("SELECT * FROM large_table");
auto result = future.get();

// 4. 트랜잭션 관리
auto tx = db->begin_transaction();
tx->execute("INSERT INTO users (name) VALUES ($1)", "Alice");
tx->commit();
```

---

## common_system 통합

**상태**: ✅ 완전 지원
**구현**: `include/kcenon/database/adapters/`, `include/kcenon/database/di/`

common_system과 함께 빌드 시 (`KCENON_HAS_COMMON_SYSTEM` 플래그), 어댑터 및 DI 통합을 제공합니다.

### IDatabase 어댑터

common_system의 `IDatabase` 인터페이스와 database_system의 `database_manager`를 연결:

```cpp
#include <kcenon/database/adapters/common_system_database_adapter.h>

auto adapter = std::make_shared<common_system_database_adapter>(
    ::database::database_types::postgresql);

// common_system IDatabase 인터페이스를 통해 사용
auto connect_result = adapter->connect("host=localhost dbname=mydb");
auto query_result = adapter->execute_query("SELECT * FROM users");
adapter->begin_transaction();
adapter->execute_command("UPDATE accounts SET balance = balance - 100");
adapter->commit();
```

### 서비스 컨테이너 등록

common_system의 의존성 주입 컨테이너에 데이터베이스 서비스 등록:

```cpp
#include <kcenon/database/di/service_registration.h>

auto& container = common::di::service_container::global();

// 사용자 정의 구성으로 등록
database_registration_config config;
config.db_type = ::database::database_types::sqlite;
config.connection_string = "database.db";
config.connect_on_register = true;
auto result = register_database_services(container, config);

// 어플리케이션 어디서든 데이터베이스 해석
auto db = container.resolve<common::interfaces::IDatabase>().value();
```

---

## C++20 모듈

**상태**: ✅ 완전 지원
**구현**: `src/modules/database.cppm`

C++20 모듈을 통한 빠른 컴파일 및 향상된 캡슐화:

| 모듈 | 파티션 | 내용 |
|------|--------|------|
| `kcenon.database` | (주 모듈) | 모든 파티션 집계 |
| `kcenon.database:core` | Core | 타입, 컨텍스트, 매니저, 백엔드 레지스트리 |
| `kcenon.database:query` | Query | 쿼리 빌더, 조건, 방언 (SQL, MongoDB, Redis) |
| `kcenon.database:backends` | Backends | PostgreSQL, SQLite, MongoDB, Redis |

```cpp
import kcenon.database;
using namespace database;

auto context = std::make_shared<database_context>();
auto manager = std::make_shared<database_manager>(context);
manager->set_mode(database_types::postgres);

auto result = manager->connect_result("host=localhost dbname=test");
if (result.is_ok()) {
    auto query_result = manager->select_query_result("SELECT * FROM users");
}
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

**최종 업데이트**: 2026-02-08
**버전**: 0.4.0.0

---

Made with ❤️ by 🍀☀🌕🌥 🌊
