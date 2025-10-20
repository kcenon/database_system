# Database System - 개선 계획

> **Language:** [English](IMPROVEMENTS.md) | **한국어**

## 현재 상태

**버전:** 1.0.0
**최종 검토:** 2025-01-20
**전체 점수:** 3.0/5

### 치명적 이슈

## 1. Connection Pool - 잠재적 커넥션 누수

**위치:** `database/connection_pool.h:262`

**현재 문제:**
```cpp
Result<std::shared_ptr<connection_wrapper>> acquire_connection() override {
    // ...
    return wrapper;  // ❌ 이후 예외 발생 시 커넥션 누수!
}
```

**문제점:**
- `acquire_connection()` 후 예외 발생 시 커넥션 누수
- RAII 래퍼 미제공

**제안된 해결책:**
```cpp
class scoped_connection {
    ~scoped_connection() {
        if (conn_ && pool_) {
            pool_->release_connection(std::move(conn_));
        }
    }
    // ...
};
```

**우선순위:** P0
**작업량:** 1-2일
**영향:** 치명적 (커넥션 고갈 방지)

---

## 고우선순위 개선사항

### 2. database_manager의 Singleton 패턴 제거

전역 상태로 인한 테스트 어려움 해결 - DI 사용

**우선순위:** P1
**작업량:** 2-3일

---

### 3. Prepared Statement 지원 추가

SQL 인젝션 방지 및 성능 향상

**우선순위:** P2
**작업량:** 5-7일

---

### 4. Transaction 지원 추가

ACID 속성을 보장하는 트랜잭션 관리

**우선순위:** P2
**작업량:** 3-4일

---

**총 작업량:** 11-16일

---

## 참고 자료

- [C++ Database Libraries](https://github.com/fffaraz/awesome-cpp#database)
- [SQL Best Practices](https://www.sqlshack.com/sql-best-practices/)
