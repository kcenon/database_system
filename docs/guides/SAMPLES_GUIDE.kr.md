---
doc_id: "DBS-GUID-017"
doc_title: "Database System Samples Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Database System Samples Guide

> **SSOT**: This document is the single source of truth for **Database System Samples Guide**.

> **Language:** [English](SAMPLES_GUIDE.md) | **한국어**

Database System의 다양한 기능을 보여주는 샘플 프로그램에 대한 종합 가이드입니다.

## 목차

- [개요](#개요)
- [기본 사용법 샘플](#기본-사용법-샘플)
- [PostgreSQL 고급 샘플](#postgresql-고급-샘플)
- [연결 풀 데모](#연결-풀-데모)
- [쿼리 빌더 예제](#쿼리-빌더-예제)
- [다중 데이터베이스 예제](#다중-데이터베이스-예제)
- [모범 사례](#모범-사례)

## 개요

Database System은 라이브러리의 여러 측면을 보여주는 여러 샘플 프로그램을 포함하고 있습니다:

| 샘플 | 파일 | 설명 | 의존성 |
|--------|------|-------------|--------------|
| 기본 사용법 | `basic_usage.cpp` | 핵심 데이터베이스 작업 | 없음 (mock 폴백) |
| PostgreSQL 고급 | `postgres_advanced.cpp` | PostgreSQL 고급 기능 | PostgreSQL (선택적) |
| 연결 풀 데모 | `connection_pool_demo.cpp` | 연결 풀링 쇼케이스 | 없음 (mock 폴백) |
| 모든 샘플 실행 | `run_all_samples.cpp` | 모든 샘플 프로그램 실행 | 없음 |

### 샘플 빌드

```bash
# 모든 샘플 빌드
mkdir build && cd build
cmake .. -DBUILD_DATABASE_SAMPLES=ON
ninja  # or make

# 샘플 실행
./bin/basic_usage
./bin/postgres_advanced
./bin/connection_pool_demo
./bin/run_all_samples
```

## 기본 사용법 샘플

**파일**: `samples/basic_usage.cpp`

database_manager 싱글톤을 사용한 기본 데이터베이스 작업을 보여줍니다.

### 시연되는 주요 기능

1. **데이터베이스 관리자 설정**
2. **연결 관리**
3. **CRUD 작업**
4. **오류 처리**
5. **Mock 구현 폴백**

### 코드 워크스루

```cpp
#include <database/database_manager.h>
#include <iostream>

int main()
{
    std::cout << "=== Database System - Basic Usage Example ===" << std::endl;

    try {
        // 1. 데이터베이스 관리자 설정
        std::cout << "\n1. Database Manager Setup:" << std::endl;
        database::database_manager& db_manager = database::database_manager::handle();

        // 데이터베이스 타입을 PostgreSQL로 설정
        if (!db_manager.set_mode(database::database_types::postgres)) {
            std::cerr << "Failed to set database mode" << std::endl;
            return 1;
        }
        std::cout << "Database type set to: PostgreSQL" << std::endl;

        // 2. 연결 관리
        std::cout << "\n2. Connection Management:" << std::endl;
        std::string connection_string =
            "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
        std::cout << "Connection string configured" << std::endl;

        std::cout << "Attempting to connect to database..." << std::endl;
        if (!db_manager.connect(connection_string)) {
            std::cout << "✗ Failed to connect to database" << std::endl;
            std::cout << "Please ensure:" << std::endl;
            std::cout << "  - PostgreSQL server is running" << std::endl;
            std::cout << "  - Database 'testdb' exists" << std::endl;
            std::cout << "  - User 'testuser' has appropriate permissions" << std::endl;
            std::cout << "  - Connection parameters are correct" << std::endl;

            std::cout << "\nTo test with a real database, update the connection string:" << std::endl;
            std::cout << "  host=your_host port=5432 dbname=your_db user=your_user password=your_pass" << std::endl;

            std::cout << "\n=== Basic Usage Example completed ===" << std::endl;
            return 0;
        }

        std::cout << "✓ Successfully connected to database" << std::endl;

        // 3. 테이블 생성 (DDL)
        std::cout << "\n3. Creating Table:" << std::endl;
        bool table_created = db_manager.create_query(
            "CREATE TABLE IF NOT EXISTS users ("
            "id SERIAL PRIMARY KEY, "
            "username VARCHAR(50) NOT NULL, "
            "email VARCHAR(100) UNIQUE, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );

        if (table_created) {
            std::cout << "✓ Table 'users' created successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to create table" << std::endl;
            return 1;
        }

        // 4. 데이터 삽입
        std::cout << "\n4. Inserting Data:" << std::endl;
        unsigned int inserted_rows = db_manager.insert_query(
            "INSERT INTO users (username, email) VALUES "
            "('john_doe', 'john@example.com'), "
            "('jane_smith', 'jane@example.com'), "
            "('bob_wilson', 'bob@example.com')"
        );

        std::cout << "✓ Inserted " << inserted_rows << " rows" << std::endl;

        // 5. 데이터 조회
        std::cout << "\n5. Selecting Data:" << std::endl;
        database::database_result users = db_manager.select_query("SELECT * FROM users ORDER BY id");

        std::cout << "Retrieved " << users.size() << " users:" << std::endl;
        std::cout << "ID | Username   | Email                | Created At" << std::endl;
        std::cout << "---|------------|----------------------|-------------------" << std::endl;

        for (const auto& user : users) {
            for (const auto& [column, value] : user) {
                std::visit([](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        std::cout << "NULL";
                    } else {
                        std::cout << v;
                    }
                }, value);
                std::cout << " | ";
            }
            std::cout << std::endl;
        }

        // 6. 데이터 업데이트
        std::cout << "\n6. Updating Data:" << std::endl;
        unsigned int updated_rows = db_manager.update_query(
            "UPDATE users SET email = 'john.doe@newdomain.com' WHERE username = 'john_doe'"
        );
        std::cout << "✓ Updated " << updated_rows << " rows" << std::endl;

        // 7. 데이터 삭제
        std::cout << "\n7. Deleting Data:" << std::endl;
        unsigned int deleted_rows = db_manager.delete_query(
            "DELETE FROM users WHERE username = 'bob_wilson'"
        );
        std::cout << "✓ Deleted " << deleted_rows << " rows" << std::endl;

        // 8. 최종 조회
        std::cout << "\n8. Final State:" << std::endl;
        database::database_result final_users = db_manager.select_query("SELECT * FROM users ORDER BY id");
        std::cout << "Final user count: " << final_users.size() << std::endl;

        // 9. 연결 해제
        if (db_manager.disconnect()) {
            std::cout << "\n✓ Successfully disconnected from database" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n=== Basic Usage Example completed ===" << std::endl;
    return 0;
}
```

### 예상 출력

```
=== Database System - Basic Usage Example ===

1. Database Manager Setup:
Database type set to: PostgreSQL
Connection string configured
Note: This example demonstrates API usage. Actual database connection requires PostgreSQL server.

2. Connection Management:
Attempting to connect to database...
✗ Failed to connect to database
Please ensure:
  - PostgreSQL server is running
  - Database 'testdb' exists
  - User 'testuser' has appropriate permissions
  - Connection parameters are correct

To test with a real database, update the connection string:
  host=your_host port=5432 dbname=your_db user=your_user password=your_pass

=== Basic Usage Example completed ===
PostgreSQL support not compiled. Connection: host=localhost port=...
```

### 학습 포인트

1. **싱글톤 패턴**: 전역 액세스를 위한 `database_manager::handle()` 사용
2. **오류 처리**: 데이터베이스를 사용할 수 없을 때 우아한 폴백
3. **CRUD 작업**: 완전한 생성, 읽기, 업데이트, 삭제 사이클
4. **Mock 지원**: 실제 데이터베이스 설치 없이 작동
5. **연결 관리**: 적절한 연결/연결 해제 생명주기

## PostgreSQL 고급 샘플

**파일**: `samples/postgres_advanced.cpp`

PostgreSQL 전용 고급 기능 및 최적화를 보여줍니다.

### 시연되는 주요 기능

1. **준비된 문(Prepared Statements)**
2. **트랜잭션**
3. **배치 작업**
4. **고급 데이터 타입**
5. **연결 풀링 통합**

### 코드 하이라이트

```cpp
// 트랜잭션 예제
bool transaction_success = db_manager.create_query("BEGIN");

try {
    // 트랜잭션 내 여러 작업
    db_manager.insert_query("INSERT INTO accounts (name, balance) VALUES ('Alice', 1000)");
    db_manager.insert_query("INSERT INTO accounts (name, balance) VALUES ('Bob', 500)");

    // 돈 이체
    db_manager.update_query("UPDATE accounts SET balance = balance - 100 WHERE name = 'Alice'");
    db_manager.update_query("UPDATE accounts SET balance = balance + 100 WHERE name = 'Bob'");

    // 트랜잭션 커밋
    db_manager.create_query("COMMIT");
    std::cout << "✓ Transaction committed successfully" << std::endl;

} catch (const std::exception& e) {
    // 오류 시 롤백
    db_manager.create_query("ROLLBACK");
    std::cerr << "✗ Transaction rolled back: " << e.what() << std::endl;
}
```

### 고급 기능

```cpp
// JSON 데이터 처리
db_manager.create_query(
    "CREATE TABLE products ("
    "id SERIAL PRIMARY KEY, "
    "name VARCHAR(100), "
    "metadata JSONB"
    ")"
);

db_manager.insert_query(
    "INSERT INTO products (name, metadata) VALUES "
    "('Laptop', '{\"brand\": \"TechCorp\", \"specs\": {\"cpu\": \"Intel i7\", \"ram\": \"16GB\"}}'), "
    "('Mouse', '{\"brand\": \"TechCorp\", \"specs\": {\"type\": \"wireless\", \"dpi\": 1600}}')"
);

// JSON 데이터 쿼리
auto products = db_manager.select_query(
    "SELECT name, metadata->>'brand' as brand, metadata->'specs'->>'cpu' as cpu "
    "FROM products WHERE metadata->>'brand' = 'TechCorp'"
);
```

## 연결 풀 데모

**파일**: `samples/connection_pool_demo.cpp`

연결 풀링 기능에 대한 종합적인 데모입니다.

### 시연되는 주요 기능

1. **풀 구성**
2. **동시 액세스**
3. **상태 모니터링**
4. **통계 추적**
5. **스레드 안전성**

### 코드 워크스루

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>
#include <thread>
#include <vector>
#include <chrono>

int main()
{
    std::cout << "=== Database System - Connection Pool Demo ===" << std::endl;

    // 1. 단일 연결 데모
    std::cout << "\n1. Single Connection Demo:" << std::endl;
    single_connection_demo();

    // 2. 다중 연결 데모
    std::cout << "\n2. Multiple Connections Demo:" << std::endl;
    multiple_connections_demo();

    // 3. 동시 작업 데모
    std::cout << "\n3. Concurrent Operations Demo:" << std::endl;
    concurrent_operations_demo();

    std::cout << "\n=== Connection Pool Demo completed ===" << std::endl;
    return 0;
}

void concurrent_operations_demo()
{
    database::database_manager& db_manager = database::database_manager::handle();

    // 연결 풀 구성
    database::connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.acquire_timeout = std::chrono::seconds(5);
    config.idle_timeout = std::chrono::seconds(30);
    config.health_check_interval = std::chrono::seconds(60);
    config.connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";

    // 연결 풀 생성
    if (!db_manager.create_connection_pool(database::database_types::postgres, config)) {
        std::cout << "✗ Failed to create connection pool" << std::endl;
        return;
    }

    // 풀 참조 가져오기
    auto pool = db_manager.get_connection_pool(database::database_types::postgres);
    if (!pool) {
        std::cout << "✗ Failed to get connection pool" << std::endl;
        return;
    }

    std::cout << "Testing concurrent database operations..." << std::endl;

    // 여러 스레드 시작
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&pool, &successful_operations, &failed_operations, i]() {
            for (int j = 0; j < 5; ++j) {
                try {
                    // 풀에서 연결 획득
                    auto connection = pool->acquire_connection();
                    if (connection) {
                        // 데이터베이스 작업 시뮬레이션
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                        // Mock 성공 작업
                        ++successful_operations;
                        std::cout << "Thread " << i << " operation " << j << " succeeded" << std::endl;

                        // 연결은 소멸 시 자동으로 풀에 반환됨
                    } else {
                        ++failed_operations;
                        std::cout << "Thread " << i << " operation " << j << " failed to acquire connection" << std::endl;
                    }
                } catch (const std::exception& e) {
                    ++failed_operations;
                    std::cout << "Thread " << i << " operation " << j << " exception: " << e.what() << std::endl;
                }
            }
        });
    }

    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Concurrent operations completed:" << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
    std::cout << "  Failed operations: " << failed_operations.load() << std::endl;
    std::cout << "  Total operations: " << (successful_operations.load() + failed_operations.load()) << std::endl;

    // 풀 통계 표시
    auto stats = db_manager.get_pool_stats();
    for (const auto& [db_type, stat] : stats) {
        std::cout << "\nPool Statistics:" << std::endl;
        std::cout << "  Total connections: " << stat.total_connections << std::endl;
        std::cout << "  Active connections: " << stat.active_connections << std::endl;
        std::cout << "  Available connections: " << stat.available_connections << std::endl;
        std::cout << "  Successful acquisitions: " << stat.successful_acquisitions << std::endl;
        std::cout << "  Failed acquisitions: " << stat.failed_acquisitions << std::endl;
    }
}
```

### 성능 인사이트

```cpp
// 연결 획득 시간 측정
auto start = std::chrono::high_resolution_clock::now();
auto connection = pool->acquire_connection();
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Connection acquired in " << duration.count() << " microseconds" << std::endl;
```

## 쿼리 빌더 예제

### SQL 쿼리 빌더 예제

```cpp
#include <database/database_manager.h>
#include <database/query_builder.h>

void sql_query_examples()
{
    database::database_manager& db = database::database_manager::handle();

    // 1. 단순 SELECT
    auto simple_select = db.create_query_builder(database::database_types::postgres)
        .select({"id", "name", "email"})
        .from("users")
        .where("status", "=", database::database_value{std::string("active")})
        .order_by("created_at", database::sort_order::desc)
        .limit(10);

    std::cout << "Simple SELECT: " << simple_select.build() << std::endl;
    // 출력: SELECT "id", "name", "email" FROM "users" WHERE status = 'active' ORDER BY created_at DESC LIMIT 10

    // 2. 복잡한 JOIN
    auto join_query = db.create_query_builder(database::database_types::postgres)
        .select({"u.name", "p.title", "p.created_at"})
        .from("users u")
        .left_join("posts p", "u.id = p.user_id")
        .where("u.status", "=", database::database_value{std::string("active")})
        .where("p.published", "=", database::database_value{true})
        .order_by("p.created_at", database::sort_order::desc);

    std::cout << "JOIN Query: " << join_query.build() << std::endl;

    // 3. 여러 값을 가진 INSERT
    auto insert_query = db.create_query_builder(database::database_types::postgres)
        .insert_into("products")
        .values({
            {
                {"name", database::database_value{std::string("Laptop")}},
                {"price", database::database_value{double(999.99)}},
                {"in_stock", database::database_value{true}}
            },
            {
                {"name", database::database_value{std::string("Mouse")}},
                {"price", database::database_value{double(29.99)}},
                {"in_stock", database::database_value{false}}
            }
        });

    std::cout << "INSERT Query: " << insert_query.build() << std::endl;

    // 4. 조건이 있는 UPDATE
    auto update_query = db.create_query_builder(database::database_types::postgres)
        .update("products")
        .set("price", database::database_value{double(899.99)})
        .set("updated_at", database::database_value{std::string("NOW()")})
        .where("name", "=", database::database_value{std::string("Laptop")});

    std::cout << "UPDATE Query: " << update_query.build() << std::endl;
}
```

### MongoDB 쿼리 빌더 예제

```cpp
void mongodb_query_examples()
{
    database::database_manager& db = database::database_manager::handle();

    // 1. 단순 Find
    auto find_query = db.create_query_builder(database::database_types::mongodb)
        .collection("users")
        .find({{"status", database::database_value{std::string("active")}}})
        .project({"name", "email"})
        .sort("created_at", -1)
        .limit(10);

    std::cout << "MongoDB Find: " << find_query.build() << std::endl;
    // 출력: db.users.find({ "status": "active" }, { "name": 1, "email": 1 }).sort({"created_at": -1}).limit(10)

    // 2. Aggregation Pipeline
    auto agg_query = db.create_query_builder(database::database_types::mongodb)
        .collection("orders")
        .match({{"status", database::database_value{std::string("completed")}}})
        .group({
            {"_id", database::database_value{std::string("$customer_id")}},
            {"total_amount", database::database_value{std::string("$sum: $amount")}},
            {"order_count", database::database_value{std::string("$sum: 1")}}
        });

    std::cout << "MongoDB Aggregation: " << agg_query.build() << std::endl;

    // 3. Insert Document
    auto insert_doc = db.create_query_builder(database::database_types::mongodb)
        .collection("products")
        .insert_one({
            {"name", database::database_value{std::string("New Product")}},
            {"price", database::database_value{double(49.99)}},
            {"category", database::database_value{std::string("electronics")}},
            {"in_stock", database::database_value{true}}
        });

    std::cout << "MongoDB Insert: " << insert_doc.build() << std::endl;
}
```

### Redis 쿼리 빌더 예제

```cpp
void redis_query_examples()
{
    database::database_manager& db = database::database_manager::handle();

    // 1. String 작업
    auto set_cmd = db.create_query_builder(database::database_types::redis)
        .set("user:123:name", "John Doe");
    std::cout << "Redis SET: " << set_cmd.build() << std::endl;
    // 출력: SET user:123:name "John Doe"

    auto get_cmd = db.create_query_builder(database::database_types::redis)
        .get("user:123:name");
    std::cout << "Redis GET: " << get_cmd.build() << std::endl;

    // 2. Hash 작업
    auto hset_cmd = db.create_query_builder(database::database_types::redis)
        .hset("user:123", "email", "john@example.com");
    std::cout << "Redis HSET: " << hset_cmd.build() << std::endl;

    auto hgetall_cmd = db.create_query_builder(database::database_types::redis)
        .hgetall("user:123");
    std::cout << "Redis HGETALL: " << hgetall_cmd.build() << std::endl;

    // 3. List 작업
    auto lpush_cmd = db.create_query_builder(database::database_types::redis)
        .lpush("notifications:123", "New message received");
    std::cout << "Redis LPUSH: " << lpush_cmd.build() << std::endl;

    auto lrange_cmd = db.create_query_builder(database::database_types::redis)
        .lrange("notifications:123", 0, 10);
    std::cout << "Redis LRANGE: " << lrange_cmd.build() << std::endl;

    // 4. Set 작업
    auto sadd_cmd = db.create_query_builder(database::database_types::redis)
        .sadd("user:123:tags", "developer");
    std::cout << "Redis SADD: " << sadd_cmd.build() << std::endl;

    auto smembers_cmd = db.create_query_builder(database::database_types::redis)
        .smembers("user:123:tags");
    std::cout << "Redis SMEMBERS: " << smembers_cmd.build() << std::endl;
}
```

## 다중 데이터베이스 예제

### 데이터베이스 추상화 예제

```cpp
void multi_database_example()
{
    database::database_manager& db = database::database_manager::handle();

    // 여러 데이터베이스에서 동일한 작업을 보여주는 함수
    auto demonstrate_select = [&db](database::database_types db_type, const std::string& db_name) {
        std::cout << "\n--- " << db_name << " Example ---" << std::endl;

        auto query = db.create_query_builder(db_type);

        switch (db_type) {
            case database::database_types::postgres:
            case database::database_types::sqlite:
                query.select({"id", "name", "email"})
                     .from("users")
                     .where("status", "=", database::database_value{std::string("active")})
                     .limit(5);
                break;

            case database::database_types::mongodb:
                query.collection("users")
                     .find({{"status", database::database_value{std::string("active")}}})
                     .project({"_id", "name", "email"})
                     .limit(5);
                break;

            case database::database_types::redis:
                query.smembers("active_users");
                break;

            default:
                return;
        }

        std::cout << "Query: " << query.build() << std::endl;
    };

    // 모든 데이터베이스 유형에 대해 시연
    demonstrate_select(database::database_types::postgres, "PostgreSQL");
    demonstrate_select(database::database_types::sqlite, "SQLite");
    demonstrate_select(database::database_types::mongodb, "MongoDB");
    demonstrate_select(database::database_types::redis, "Redis");
}
```

### 예상 출력

```
--- PostgreSQL Example ---
Query: SELECT "id", "name", "email" FROM "users" WHERE status = 'active' LIMIT 5

--- SQLite Example ---
Query: SELECT [id], [name], [email] FROM [users] WHERE status = 'active' LIMIT 5

--- MongoDB Example ---
Query: db.users.find({ "status": "active" }, { "_id": 1, "name": 1, "email": 1 }).limit(5)

--- Redis Example ---
Query: SMEMBERS active_users
```

## 모범 사례

### 1. 오류 처리

```cpp
try {
    database::database_manager& db = database::database_manager::handle();

    // 항상 반환 값 확인
    if (!db.set_mode(database::database_types::postgres)) {
        throw std::runtime_error("Failed to set database mode");
    }

    if (!db.connect(connection_string)) {
        throw std::runtime_error("Failed to connect to database");
    }

    // 자동 정리를 위한 RAII 사용
    auto result = db.select_query("SELECT * FROM users");

    // 결과 처리...

} catch (const std::exception& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    // 오류를 적절히 처리
}
```

### 2. 연결 풀 사용법

```cpp
void efficient_pool_usage()
{
    database::database_manager& db = database::database_manager::handle();

    // 풀을 한 번 구성
    database::connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 20;
    config.connection_string = "your_connection_string";

    db.create_connection_pool(database::database_types::postgres, config);

    // 여러 작업에 풀 사용
    auto pool = db.get_connection_pool(database::database_types::postgres);

    {
        // 연결이 풀에 자동으로 반환됨
        auto conn = pool->acquire_connection();
        if (conn) {
            auto result = conn->select_query("SELECT * FROM users");
            // 결과 처리...
        }
    } // 여기서 연결 반환

    // 풀 상태 모니터링
    auto stats = db.get_pool_stats();
    if (stats[database::database_types::postgres].failed_acquisitions > 10) {
        // 풀 재구성 고려
    }
}
```

### 3. 쿼리 빌더 모범 사례

```cpp
void query_builder_best_practices()
{
    database::database_manager& db = database::database_manager::handle();

    // 1. 타입이 지정된 값 사용
    auto query = db.create_query_builder(database::database_types::postgres)
        .select({"id", "name", "created_at"})
        .from("users")
        .where("age", ">=", database::database_value{int64_t(18)})  // 타입이 지정된 값
        .where("active", "=", database::database_value{true})       // Boolean
        .where("name", "LIKE", database::database_value{std::string("%john%")});  // String

    // 2. 복잡한 조건 처리
    auto complex_query = db.create_query_builder(database::database_types::postgres)
        .select({"*"})
        .from("orders")
        .where_raw("(status = 'pending' OR status = 'processing') AND amount > 100");

    // 3. 적절한 데이터베이스별 기능 사용
    if (db.database_type() == database::database_types::postgres) {
        query.where_raw("metadata @> '{\"premium\": true}'");  // PostgreSQL JSON
    }

    // 4. 빌더 재설정 및 재사용
    query.reset();
    query.select({"count(*)"})
         .from("users")
         .where("created_at", ">", database::database_value{std::string("2023-01-01")});
}
```

### 4. 성능 최적화

```cpp
void performance_optimization()
{
    database::database_manager& db = database::database_manager::handle();

    // 1. 동시 액세스를 위한 연결 풀링 사용
    database::connection_pool_config config;
    config.min_connections = 10;
    config.max_connections = 50;
    db.create_connection_pool(database::database_types::postgres, config);

    // 2. 가능한 경우 배치 작업
    auto batch_insert = db.create_query_builder(database::database_types::postgres)
        .insert_into("logs")
        .values({
            {{"level", database::database_value{std::string("INFO")}},
             {"message", database::database_value{std::string("Operation 1")}}},
            {{"level", database::database_value{std::string("ERROR")}},
             {"message", database::database_value{std::string("Operation 2")}}},
            // ... 더 많은 행
        });

    // 3. DDL에서 적절한 인덱스 사용
    db.create_query("CREATE INDEX idx_users_email ON users(email)");
    db.create_query("CREATE INDEX idx_users_created_at ON users(created_at)");

    // 4. 통계에 기반한 모니터링 및 튜닝
    auto stats = db.get_pool_stats();
    for (const auto& [db_type, stat] : stats) {
        double success_rate = static_cast<double>(stat.successful_acquisitions) /
                             (stat.successful_acquisitions + stat.failed_acquisitions);

        if (success_rate < 0.95) {
            std::cout << "Warning: Low success rate (" << success_rate << "), consider increasing pool size" << std::endl;
        }
    }
}
```

---

이러한 샘플은 다양한 시나리오에서 Database System을 사용하는 종합적인 예제를 제공합니다. 더 고급 사용 사례 및 특정 데이터베이스 기능에 대해서는 [API Reference](API_REFERENCE.md) 및 개별 데이터베이스 문서를 참조하십시오.

---

*Last Updated: 2025-10-20*
