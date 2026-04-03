---
doc_id: "DBS-API-001"
doc_title: "Database System API Reference"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "API"
---

# Database System API Reference

> **SSOT**: This document is the single source of truth for **Database System API Reference**.

> **Language:** [English](API_REFERENCE.md) | **한국어**

멀티 백엔드 지원, 연결 풀링, 쿼리 빌더를 갖춘 Database System C++20 라이브러리의 완전한 API 레퍼런스입니다.

## 목차

- [핵심 클래스](#핵심-클래스)
- [Database Manager](#database-manager)
- [연결 풀링](#연결-풀링)
- [쿼리 빌더](#쿼리-빌더)
- [ORM 프레임워크](#orm-프레임워크)
- [성능 모니터링](#성능-모니터링)
- [보안 프레임워크](#보안-프레임워크)
- [비동기 작업](#비동기-작업)
- [데이터베이스 타입](#데이터베이스-타입)
- [C++20 Concepts](#c20-concepts)
- [에러 처리](#에러-처리)
- [예제](#예제)

## 핵심 클래스

### database_base

모든 데이터베이스 구현의 추상 기본 클래스입니다.

```cpp
class database_base
{
public:
    virtual ~database_base() = default;

    // Database identification
    virtual database_types database_type() = 0;

    // Connection management
    virtual bool connect(const std::string& connection_string) = 0;
    virtual bool disconnect() = 0;

    // Query operations
    virtual bool create_query(const std::string& query_string) = 0;
    virtual unsigned int insert_query(const std::string& query_string) = 0;
    virtual unsigned int update_query(const std::string& query_string) = 0;
    virtual unsigned int delete_query(const std::string& query_string) = 0;
    virtual database_result select_query(const std::string& query_string) = 0;
};
```

### database_manager

데이터베이스 연결 및 작업을 관리하는 싱글톤 클래스입니다.

```cpp
class database_manager
{
public:
    // Singleton access
    static database_manager& handle();

    // Database configuration
    bool set_mode(const database_types& database_type);
    database_types database_type();

    // Connection management
    bool connect(const std::string& connection_string);
    bool disconnect();

    // Query operations
    bool create_query(const std::string& query_string);
    unsigned int insert_query(const std::string& query_string);
    unsigned int update_query(const std::string& query_string);
    unsigned int delete_query(const std::string& query_string);
    database_result select_query(const std::string& query_string);

    // Phase 3: Advanced Features
    bool create_connection_pool(database_types db_type, const connection_pool_config& config);
    std::shared_ptr<connection_pool_base> get_connection_pool(database_types db_type);
    std::map<database_types, connection_stats> get_pool_stats() const;
    query_builder create_query_builder();
    query_builder create_query_builder(database_types db_type);

    // Phase 4: Enterprise Features
    bool execute_query(const std::string& query_string);
    std::shared_ptr<orm::entity_manager> get_entity_manager();
    std::shared_ptr<monitoring::performance_monitor> get_performance_monitor();
    std::shared_ptr<security::access_control> get_access_control();
    std::shared_ptr<async::async_database> get_async_database();
};
```

## Database Manager

### 기본 사용법

```cpp
#include <database/database_manager.h>
using namespace database;

// Get singleton instance
database_manager& db = database_manager::handle();

// Set database type
db.set_mode(database_types::postgres);

// Connect
std::string conn_str = "host=localhost port=5432 dbname=test user=admin password=secret";
if (!db.connect(conn_str)) {
    // Handle connection error
}

// Execute queries
db.create_query("CREATE TABLE users (id SERIAL PRIMARY KEY, name VARCHAR(100))");
unsigned int rows = db.insert_query("INSERT INTO users (name) VALUES ('John')");
database_result result = db.select_query("SELECT * FROM users");
```

### 지원 메서드

| 메서드 | 설명 | 반환값 |
|--------|------|--------|
| `set_mode(database_types)` | 데이터베이스 백엔드 타입 설정 | `bool` 성공 여부 |
| `database_type()` | 현재 데이터베이스 타입 반환 | `database_types` |
| `connect(connection_string)` | 데이터베이스 연결 | `bool` 성공 여부 |
| `disconnect()` | 데이터베이스 연결 해제 | `bool` 성공 여부 |
| `create_query(query)` | DDL 쿼리 실행 | `bool` 성공 여부 |
| `insert_query(query)` | INSERT 쿼리 실행 | `unsigned int` 영향받은 행 수 |
| `update_query(query)` | UPDATE 쿼리 실행 | `unsigned int` 영향받은 행 수 |
| `delete_query(query)` | DELETE 쿼리 실행 | `unsigned int` 영향받은 행 수 |
| `select_query(query)` | SELECT 쿼리 실행 | `database_result` |

## 연결 풀링

### connection_pool_config

연결 풀의 구성 구조체입니다.

```cpp
struct connection_pool_config
{
    size_t min_connections = 2;                              // Minimum connections to maintain
    size_t max_connections = 20;                             // Maximum connections allowed
    std::chrono::milliseconds acquire_timeout{5000};         // Timeout for acquiring connections
    std::chrono::milliseconds idle_timeout{30000};           // Timeout for idle connections
    std::chrono::milliseconds health_check_interval{60000};   // Health check interval
    bool enable_health_checks = true;                        // Enable periodic health checks
    std::string connection_string;                           // Database connection string
};
```

### connection_stats

연결 풀 모니터링을 위한 통계 구조체입니다.

```cpp
struct connection_stats
{
    size_t total_connections = 0;                             // Total connections created
    size_t active_connections = 0;                            // Currently active connections
    size_t available_connections = 0;                         // Available connections in pool
    size_t failed_acquisitions = 0;                           // Number of failed acquisitions
    size_t successful_acquisitions = 0;                       // Number of successful acquisitions
    std::chrono::steady_clock::time_point last_health_check;  // Last health check time
};
```

### connection_pool_base

연결 풀의 추상 기본 클래스입니다.

```cpp
class connection_pool_base
{
public:
    virtual ~connection_pool_base() = default;

    virtual std::shared_ptr<connection_wrapper> acquire_connection() = 0;
    virtual void release_connection(std::shared_ptr<connection_wrapper> connection) = 0;
    virtual size_t active_connections() const = 0;
    virtual size_t available_connections() const = 0;
    virtual connection_stats get_stats() const = 0;
    virtual void shutdown() = 0;
};
```

### 사용 예제

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>

// Configure connection pool
connection_pool_config config;
config.min_connections = 5;
config.max_connections = 20;
config.acquire_timeout = std::chrono::seconds(5);
config.connection_string = "host=localhost port=5432 dbname=test user=admin password=secret";

// Create pool
database_manager& db = database_manager::handle();
if (!db.create_connection_pool(database_types::postgres, config)) {
    // Handle error
}

// Use pool
auto pool = db.get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();

if (connection) {
    // Use connection
    auto result = connection->select_query("SELECT * FROM users");

    // Connection automatically returned to pool when destroyed
}

// Monitor statistics
auto stats = db.get_pool_stats();
for (const auto& [db_type, stat] : stats) {
    std::cout << "Active: " << stat.active_connections
              << " Available: " << stat.available_connections << std::endl;
}
```

## 쿼리 빌더

### query_builder

다양한 데이터베이스 타입에 적응하는 범용 쿼리 빌더입니다.

```cpp
class query_builder
{
public:
    explicit query_builder(database_types db_type = database_types::none);

    // Database type selection
    query_builder& for_database(database_types db_type);

    // SQL-style interface (PostgreSQL, SQLite)
    query_builder& select(const std::vector<std::string>& columns);
    query_builder& from(const std::string& table);
    query_builder& where(const std::string& field, const std::string& op, const database_value& value);
    query_builder& join(const std::string& table, const std::string& condition);
    query_builder& order_by(const std::string& column, sort_order order = sort_order::asc);
    query_builder& limit(size_t count);

    // NoSQL-style interface
    query_builder& collection(const std::string& name); // MongoDB
    query_builder& key(const std::string& key);         // Redis

    // Universal operations
    query_builder& insert(const std::map<std::string, database_value>& data);
    query_builder& update(const std::map<std::string, database_value>& data);
    query_builder& remove(); // DELETE/DROP

    // Build and execute
    std::string build() const;
    database_result execute(database_base* db) const;

    // Reset builder
    void reset();
};
```

### sql_query_builder

SQL 데이터베이스를 위한 특화된 쿼리 빌더입니다.

```cpp
class sql_query_builder
{
public:
    // SELECT operations
    sql_query_builder& select(const std::vector<std::string>& columns);
    sql_query_builder& select(const std::string& column);
    sql_query_builder& select_raw(const std::string& raw_select);
    sql_query_builder& from(const std::string& table);

    // WHERE conditions
    sql_query_builder& where(const std::string& field, const std::string& op, const database_value& value);
    sql_query_builder& where(const query_condition& condition);
    sql_query_builder& where_raw(const std::string& raw_where);
    sql_query_builder& or_where(const std::string& field, const std::string& op, const database_value& value);

    // JOIN operations
    sql_query_builder& join(const std::string& table, const std::string& condition, join_type type = join_type::inner);
    sql_query_builder& left_join(const std::string& table, const std::string& condition);
    sql_query_builder& right_join(const std::string& table, const std::string& condition);

    // GROUP BY and HAVING
    sql_query_builder& group_by(const std::vector<std::string>& columns);
    sql_query_builder& group_by(const std::string& column);
    sql_query_builder& having(const std::string& condition);

    // ORDER BY
    sql_query_builder& order_by(const std::string& column, sort_order order = sort_order::asc);
    sql_query_builder& order_by_raw(const std::string& raw_order);

    // LIMIT and OFFSET
    sql_query_builder& limit(size_t count);
    sql_query_builder& offset(size_t count);

    // INSERT operations
    sql_query_builder& insert_into(const std::string& table);
    sql_query_builder& values(const std::map<std::string, database_value>& data);
    sql_query_builder& values(const std::vector<std::map<std::string, database_value>>& rows);

    // UPDATE operations
    sql_query_builder& update(const std::string& table);
    sql_query_builder& set(const std::string& field, const database_value& value);
    sql_query_builder& set(const std::map<std::string, database_value>& data);

    // DELETE operations
    sql_query_builder& delete_from(const std::string& table);

    // Build final query
    std::string build() const;
    std::string build_for_database(database_types db_type) const;

    // Reset builder
    void reset();
};
```

### mongodb_query_builder

MongoDB를 위한 특화된 쿼리 빌더입니다.

```cpp
class mongodb_query_builder
{
public:
    // Collection operations
    mongodb_query_builder& collection(const std::string& name);

    // Find operations
    mongodb_query_builder& find(const std::map<std::string, database_value>& filter = {});
    mongodb_query_builder& find_one(const std::map<std::string, database_value>& filter = {});

    // Projection
    mongodb_query_builder& project(const std::vector<std::string>& fields);
    mongodb_query_builder& exclude(const std::vector<std::string>& fields);

    // Sorting
    mongodb_query_builder& sort(const std::map<std::string, int>& sort_spec);
    mongodb_query_builder& sort(const std::string& field, int direction = 1);

    // Limit and Skip
    mongodb_query_builder& limit(size_t count);
    mongodb_query_builder& skip(size_t count);

    // Insert operations
    mongodb_query_builder& insert_one(const std::map<std::string, database_value>& document);
    mongodb_query_builder& insert_many(const std::vector<std::map<std::string, database_value>>& documents);

    // Update operations
    mongodb_query_builder& update_one(const std::map<std::string, database_value>& filter,
                                     const std::map<std::string, database_value>& update);
    mongodb_query_builder& update_many(const std::map<std::string, database_value>& filter,
                                      const std::map<std::string, database_value>& update);

    // Delete operations
    mongodb_query_builder& delete_one(const std::map<std::string, database_value>& filter);
    mongodb_query_builder& delete_many(const std::map<std::string, database_value>& filter);

    // Aggregation pipeline
    mongodb_query_builder& match(const std::map<std::string, database_value>& conditions);
    mongodb_query_builder& group(const std::map<std::string, database_value>& group_spec);
    mongodb_query_builder& unwind(const std::string& field);

    // Build final query
    std::string build() const;
    std::string build_json() const;

    // Reset builder
    void reset();
};
```

### redis_query_builder

Redis를 위한 특화된 쿼리 빌더입니다.

```cpp
class redis_query_builder
{
public:
    // String operations
    redis_query_builder& set(const std::string& key, const std::string& value);
    redis_query_builder& get(const std::string& key);
    redis_query_builder& del(const std::string& key);
    redis_query_builder& exists(const std::string& key);

    // Hash operations
    redis_query_builder& hset(const std::string& key, const std::string& field, const std::string& value);
    redis_query_builder& hget(const std::string& key, const std::string& field);
    redis_query_builder& hdel(const std::string& key, const std::string& field);
    redis_query_builder& hgetall(const std::string& key);

    // List operations
    redis_query_builder& lpush(const std::string& key, const std::string& value);
    redis_query_builder& rpush(const std::string& key, const std::string& value);
    redis_query_builder& lpop(const std::string& key);
    redis_query_builder& rpop(const std::string& key);
    redis_query_builder& lrange(const std::string& key, int start, int stop);

    // Set operations
    redis_query_builder& sadd(const std::string& key, const std::string& member);
    redis_query_builder& srem(const std::string& key, const std::string& member);
    redis_query_builder& sismember(const std::string& key, const std::string& member);
    redis_query_builder& smembers(const std::string& key);

    // Expiration
    redis_query_builder& expire(const std::string& key, int seconds);
    redis_query_builder& ttl(const std::string& key);

    // Build command
    std::string build() const;
    std::vector<std::string> build_args() const;

    // Reset builder
    void reset();
};
```

### 쿼리 빌더 예제

```cpp
// SQL Query Builder
auto sql_query = db.create_query_builder(database_types::postgres)
    .select({"name", "email", "created_at"})
    .from("users")
    .where("age", ">", database_value{int64_t(18)})
    .where("status", "=", database_value{std::string("active")})
    .order_by("created_at", sort_order::desc)
    .limit(10);

std::string query = sql_query.build();
// Output: SELECT "name", "email", "created_at" FROM "users" WHERE age > 18 AND status = 'active' ORDER BY created_at DESC LIMIT 10

// MongoDB Query Builder
auto mongo_query = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .find({{"status", database_value{std::string("active")}}})
    .sort("created_at", -1)
    .limit(10);

std::string mongo_cmd = mongo_query.build();
// Output: db.users.find({ "status": "active" }).sort({"created_at": -1}).limit(10)

// Redis Query Builder
auto redis_query = db.create_query_builder(database_types::redis)
    .hget("user:123", "email");

std::string redis_cmd = redis_query.build();
// Output: HGET user:123 email
```

## 데이터베이스 타입

### database_types

지원되는 데이터베이스 타입의 열거형입니다.

```cpp
enum class database_types : uint8_t
{
    none = 0,           // No database backend
    postgres = 1,       // PostgreSQL backend
    sqlite = 3,         // SQLite backend
    oracle = 4,         // Oracle backend (future)
    mongodb = 5,        // MongoDB backend
    redis = 6           // Redis backend
};
```

### database_value

데이터베이스 값을 위한 variant 타입입니다.

```cpp
using database_value = std::variant<std::string, int64_t, double, bool, std::monostate>;
```

### database_result

데이터베이스 결과를 위한 타입 정의입니다.

```cpp
using database_row = std::map<std::string, database_value>;
using database_result = std::vector<database_row>;
```

### database_value 사용하기

```cpp
// Creating values
database_value str_val{std::string("hello")};
database_value int_val{int64_t(42)};
database_value double_val{3.14};
database_value bool_val{true};
database_value null_val{std::monostate{}};

// Visiting values
std::visit([](const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::string>) {
        std::cout << "String: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        std::cout << "Integer: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, double>) {
        std::cout << "Double: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, bool>) {
        std::cout << "Boolean: " << (value ? "true" : "false") << std::endl;
    } else if constexpr (std::is_same_v<T, std::monostate>) {
        std::cout << "NULL" << std::endl;
    }
}, str_val);
```

## C++20 Concepts

database_system은 컴파일 타임 타입 검증을 위한 C++20 concepts를 제공하여, 더 명확한 오류 메시지, 자체 문서화 코드, 더 나은 IDE 지원을 제공합니다.

### 개요

**헤더**: `#include <database/core/concepts.h>`
**네임스페이스**: `database::concepts`

### Concepts 사용의 이점

- **더 명확한 오류 메시지**: 템플릿 오류가 수백 줄의 SFINAE 실패 대신 concept 위반으로 표시됨
- **자체 문서화 코드**: Concepts가 타입 요구사항을 명시적으로 표현
- **더 나은 IDE 지원**: 더 정확한 자동 완성 및 타입 힌트
- **코드 단순화**: `std::enable_if` 보일러플레이트 제거

### 호출 가능 Concepts

| Concept | 설명 | 시그니처 |
|---------|------|----------|
| `Invocable<F, Args...>` | 주어진 인자로 호출 가능 | `F(Args...)` |
| `VoidCallable<F, Args...>` | void를 반환하는 호출 가능 | `void F(Args...)` |
| `ReturnsResult<F, R, Args...>` | R 타입을 반환하는 호출 가능 | `R F(Args...)` |
| `Predicate<F, Args...>` | bool을 반환하는 호출 가능 | `bool F(Args...)` |
| `NoexceptCallable<F, Args...>` | noexcept로 표시된 호출 가능 | `noexcept F(Args...)` |
| `DelayedCallable<F>` | 지연 실행을 위한 호출 가능 | `void F()` + 이동 생성 가능 |
| `AsyncCallable<F, R>` | 비동기 실행을 위한 호출 가능 | `R F()` |

### 데이터베이스 전용 Concepts

| Concept | 설명 | 시그니처 |
|---------|------|----------|
| `QueryCallback<F, ResultType>` | 쿼리 결과 처리 | `void F(ResultType)` |
| `ErrorHandler<F>` | 데이터베이스 오류 처리 | `void F(const std::exception&)` |
| `ConnectionFactory<F>` | 데이터베이스 연결 생성 | `std::unique_ptr<database_base> F()` |
| `BackendFactory<F>` | 데이터베이스 백엔드 생성 | `std::unique_ptr<database_backend> F()` |

### 스트림 Concepts

| Concept | 설명 | 시그니처 |
|---------|------|----------|
| `StreamEventHandler<F, EventType>` | 스트림 이벤트 처리 | `void F(const EventType&)` |
| `StreamEventFilter<F, EventType>` | 스트림 이벤트 필터링 | `bool F(const EventType&)` |

### 트랜잭션 Concepts

| Concept | 설명 | 용도 |
|---------|------|------|
| `TransactionAction<F>` | 트랜잭션 액션 | Saga 패턴 정방향 액션 |
| `CompensationAction<F>` | 보상 (롤백) 액션 | Saga 패턴 롤백 액션 |

### 태스크 실행 Concepts

| Concept | 설명 | 용도 |
|---------|------|------|
| `SubmittableTask<F, Args...>` | 비동기 executor 제출용 호출 가능 | `async_executor.submit()` |
| `VoidTask<F, Args...>` | Fire-and-forget 호출 가능 | 백그라운드 태스크 |

### 풀 Concepts

| Concept | 설명 | 요구사항 |
|---------|------|----------|
| `PooledResource<T>` | 풀에서 관리되는 리소스 | 클래스 타입, 기본 생성 가능 |
| `ConnectionWrapper<T>` | 데이터베이스 연결 래퍼 | `get()`이 `database_base*` 반환, `is_valid()`가 bool 반환 |

### 사용 예제

#### 타입 안전 비동기 태스크 제출

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// 타입 안전 태스크 제출을 위한 concept 제약 함수
template<SubmittableTask<int> F>
auto submit_computation(async_executor& executor, F&& func) {
    return executor.submit(std::forward<F>(func));
}

// 사용법
auto future = submit_computation(executor, []() { return 42; });
```

#### 쿼리 콜백 등록

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// 타입 안전 쿼리 콜백 등록
template<QueryCallback<database_result> F>
void on_query_complete(F&& callback) {
    query_callbacks_.push_back(std::forward<F>(callback));
}

// 사용법
on_query_complete([](const database_result& result) {
    std::cout << "쿼리가 " << result.size() << " 행을 반환했습니다" << std::endl;
});
```

#### Concept 제약이 있는 에러 핸들러

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// 컴파일 타임 타입 검증이 있는 에러 핸들러 설정
template<ErrorHandler F>
void set_error_handler(F&& handler) {
    error_handler_ = std::forward<F>(handler);
}

// 사용법
set_error_handler([](const std::exception& e) {
    std::cerr << "데이터베이스 오류: " << e.what() << std::endl;
});
```

#### 스트림 이벤트 처리

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// concept 제약이 있는 이벤트 핸들러 등록
template<StreamEventHandler<stream_event> F>
void register_handler(const std::string& channel, F&& handler) {
    handlers_[channel] = std::forward<F>(handler);
}

// concept 제약이 있는 이벤트 필터 등록
template<StreamEventFilter<stream_event> F>
void add_filter(const std::string& channel, F&& filter) {
    filters_[channel] = std::forward<F>(filter);
}

// 사용법
register_handler("user_updates", [](const stream_event& event) {
    process_user_update(event);
});

add_filter("user_updates", [](const stream_event& event) {
    return event.type == "INSERT" || event.type == "UPDATE";
});
```

#### 트랜잭션 Concepts를 사용한 Saga 패턴

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// concept 제약이 있는 saga 단계 추가
template<TransactionAction A, CompensationAction C>
void add_saga_step(A&& action, C&& compensation) {
    steps_.emplace_back(
        std::forward<A>(action),
        std::forward<C>(compensation)
    );
}

// 사용법
saga_builder builder;
builder.add_step(
    []() { /* 주문 생성 */ },
    []() { /* 주문 취소 */ }
);
builder.add_step(
    []() { /* 재고 예약 */ },
    []() { /* 재고 해제 */ }
);
```

### Concept 제약이 있는 API 메서드

다음 메서드들은 이제 C++20 concept 제약을 가집니다:

| 클래스 | 메서드 | Concept 제약 |
|--------|--------|--------------|
| `async_executor` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `async_executor_v2` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `thread_adapter` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `async_result<T>` | `then()` | `concepts::VoidCallable<T>` |
| `async_result<T>` | `on_error()` | `concepts::ErrorHandler` |
| `stream_processor` | `register_event_handler()` | `concepts::StreamEventHandler<stream_event>` |
| `stream_processor` | `register_global_handler()` | `concepts::StreamEventHandler<stream_event>` |
| `stream_processor` | `add_event_filter()` | `concepts::StreamEventFilter<stream_event>` |
| `saga_builder` | `add_step()` | `concepts::TransactionAction`, `concepts::CompensationAction` |

**참고:** 기존 `std::function` 오버로드는 하위 호환성을 위해 유지됩니다.

## 에러 처리

### 예외 안전성

모든 데이터베이스 작업은 RAII 리소스 관리를 통해 예외 안전성을 제공합니다.

```cpp
try {
    database_manager& db = database_manager::handle();

    if (!db.set_mode(database_types::postgres)) {
        throw std::runtime_error("Failed to set database mode");
    }

    if (!db.connect(connection_string)) {
        throw std::runtime_error("Failed to connect to database");
    }

    auto result = db.select_query("SELECT * FROM users");
    // Process result

} catch (const std::exception& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
}
```

### Mock 구현

데이터베이스 라이브러리가 사용 불가능한 경우, 시스템은 빈 결과를 반환하지만 예외를 발생시키지 않는 mock 구현을 제공합니다.

```cpp
// Even without PostgreSQL libraries, this won't crash
database_manager& db = database_manager::handle();
db.set_mode(database_types::postgres);

if (!db.connect("mock://connection")) {
    // This will fail gracefully with mock implementation
    std::cout << "Mock connection - no actual database required" << std::endl;
}

auto result = db.select_query("SELECT * FROM users");
// Returns empty result with mock implementation
```

## 예제

### 완전한 사용 예제

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>
#include <database/query_builder.h>
#include <iostream>

int main() {
    try {
        // Initialize database manager
        database_manager& db = database_manager::handle();

        // Configure PostgreSQL
        if (!db.set_mode(database_types::postgres)) {
            std::cerr << "Failed to set database mode" << std::endl;
            return 1;
        }

        // Setup connection pool
        connection_pool_config config;
        config.min_connections = 2;
        config.max_connections = 10;
        config.connection_string = "host=localhost port=5432 dbname=test user=admin password=secret";

        if (!db.create_connection_pool(database_types::postgres, config)) {
            std::cerr << "Failed to create connection pool" << std::endl;
            return 1;
        }

        // Create table using query builder
        auto create_table = db.create_query_builder()
            .create_raw("CREATE TABLE IF NOT EXISTS users ("
                       "id SERIAL PRIMARY KEY, "
                       "name VARCHAR(100) NOT NULL, "
                       "email VARCHAR(100) UNIQUE, "
                       "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");

        // Insert data
        auto insert_query = db.create_query_builder()
            .insert_into("users")
            .values({
                {"name", database_value{std::string("John Doe")}},
                {"email", database_value{std::string("john@example.com")}}
            });

        // Select data with conditions
        auto select_query = db.create_query_builder()
            .select({"id", "name", "email", "created_at"})
            .from("users")
            .where("name", "LIKE", database_value{std::string("%John%")})
            .order_by("created_at", sort_order::desc)
            .limit(10);

        // Execute queries
        auto pool = db.get_connection_pool(database_types::postgres);
        auto connection = pool->acquire_connection();

        if (connection) {
            // Execute through connection
            auto result = select_query.execute(connection.get());

            // Process results
            for (const auto& row : result) {
                for (const auto& [column, value] : row) {
                    std::cout << column << ": ";
                    std::visit([](const auto& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::monostate>) {
                            std::cout << "NULL";
                        } else {
                            std::cout << v;
                        }
                    }, value);
                    std::cout << " ";
                }
                std::cout << std::endl;
            }
        }

        // Monitor pool statistics
        auto stats = db.get_pool_stats();
        for (const auto& [db_type, stat] : stats) {
            std::cout << "Pool Stats - Active: " << stat.active_connections
                      << " Available: " << stat.available_connections << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### 다중 데이터베이스 예제

```cpp
#include <database/database_manager.h>
#include <database/query_builder.h>

int main() {
    database_manager& db = database_manager::handle();

    // PostgreSQL operations
    auto pg_query = db.create_query_builder(database_types::postgres)
        .select({"id", "name"})
        .from("users")
        .where("status", "=", database_value{std::string("active")});

    std::cout << "PostgreSQL: " << pg_query.build() << std::endl;

    // MongoDB operations
    auto mongo_query = db.create_query_builder(database_types::mongodb)
        .collection("users")
        .find({{"status", database_value{std::string("active")}}})
        .project({"_id", "name"});

    std::cout << "MongoDB: " << mongo_query.build() << std::endl;

    // Redis operations
    auto redis_query = db.create_query_builder(database_types::redis)
        .hgetall("user:123");

    std::cout << "Redis: " << redis_query.build() << std::endl;

    return 0;
}
```

---

## Phase 4: Enterprise APIs

### ORM 프레임워크

```cpp
#include <database/orm/entity.h>

// Entity definition
class User : public entity_base {
    ENTITY_TABLE("users")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | index("idx_username"))
    ENTITY_FIELD(std::string, email, unique())
    ENTITY_METADATA()
};

// Entity operations
entity_manager::instance().create_tables(db);
auto users = User::query(db).where("age > 18").execute();
```

### 성능 모니터링

```cpp
#include <database/monitoring/performance_monitor.h>

// Performance monitoring
auto& monitor = performance_monitor::instance();
monitor.set_alert_thresholds(0.05, std::chrono::milliseconds(1000));
auto summary = monitor.get_performance_summary();
```

### 보안 프레임워크

```cpp
#include <database/security/secure_connection.h>

// Access control
auto& access = access_control::instance();
access.create_role(admin_role);
bool allowed = access.check_permission("user123", "users", "SELECT");

// Audit logging
AUDIT_LOG_ACCESS("user123", "session456", "SELECT", "users", "query_hash", true, "");
```

### 비동기 작업

```cpp
#include <database/async/async_operations.h>

// Coroutine support
database_awaitable<bool> async_operation() {
    auto result = co_await async_db.execute_coro("SELECT * FROM users");
    co_return result;
}

// Distributed transactions
auto& coordinator = transaction_coordinator::instance();
auto tx_id = coordinator.begin_distributed_transaction({db1, db2});
```

---

## 시스템 요구사항

- **C++ 표준**: C++20
- **지원 컴파일러**: GCC 10+, Clang 11+, MSVC 2019+
- **지원 플랫폼**: Windows, macOS, Linux

최신 API 업데이트 및 변경사항은 [CHANGELOG](../CHANGELOG.md)를 참조하세요.

---

*Last Updated: 2025-12-09*
