# 데이터베이스 게이트웨이 API 레퍼런스

## 개요

데이터베이스 게이트웨이는 라우팅, 캐싱, 로드 밸런싱 기능이 내장된
데이터베이스 작업을 위한 중앙 집중식 접근 지점을 제공합니다.

## 빠른 시작

```cpp
#include <database/gateway/database_gateway.h>

using namespace database::gateway;

// 게이트웨이 생성
database_gateway gateway;

// 설정 및 시작
auto start_result = gateway.start(8080, security_config{});
if (start_result.is_err()) {
    std::cerr << "게이트웨이 시작 실패" << std::endl;
    return 1;
}

// 쿼리 실행
auto result = gateway.execute_query("SELECT * FROM users WHERE active = true");
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        // 행 처리
    }
}

// 게이트웨이 종료
gateway.stop();
```

## 설정

### routing_strategy (라우팅 전략)

- `ROUND_ROBIN`: 백엔드 간 쿼리를 균등하게 분배
- `LEAST_CONNECTIONS`: 활성 연결이 가장 적은 백엔드로 라우팅
- `RANDOM`: 무작위 백엔드 선택
- `WEIGHTED`: 가중치 기반 분배
- `LATENCY_BASED`: 지연 시간이 가장 낮은 백엔드로 라우팅

### cache_config (캐시 설정)

| 필드 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| enabled | bool | true | 쿼리 캐싱 활성화 |
| max_entries | size_t | 10000 | 최대 캐시 항목 수 |
| default_ttl | seconds | 300 | 캐시 TTL (초) |
| eviction_policy | eviction_policy | LRU | 캐시 퇴거 정책 |

### audit_config (감사 로깅 설정)

| 필드 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| enabled | bool | true | 감사 로깅 활성화 |
| log_all_queries | bool | false | 모든 쿼리 로깅 |
| log_slow_queries_ms | uint32_t | 1000 | 느린 쿼리 임계값 (ms) |
| log_failed_queries | bool | true | 실패한 쿼리 로깅 |

## API 레퍼런스

### 라이프사이클 메서드

#### start()
```cpp
result<void> start(uint16_t port, const security_config& security);
```
지정된 포트에서 게이트웨이 서버를 시작합니다.

**매개변수**:
- `port`: 수신할 포트 번호
- `security`: 보안 설정 (TLS, 인증)

**반환값**: 성공 또는 실패를 나타내는 `result<void>`.

#### stop()
```cpp
void stop();
```
게이트웨이를 우아하게 종료하고 모든 연결을 닫습니다.

#### is_running()
```cpp
bool is_running() const;
```
게이트웨이가 현재 실행 중인지 반환합니다.

### 클러스터 관리

#### set_cluster()
```cpp
void set_cluster(std::shared_ptr<distributed::cluster_manager> cluster);
```
분산 쿼리 라우팅을 위한 클러스터 매니저를 할당합니다.

### 쿼리 실행

#### execute_query()
```cpp
result<core::database_result> execute_query(const std::string& query);
```
쿼리 유형에 따라 자동 라우팅으로 쿼리를 실행합니다.

**매개변수**:
- `query`: SQL 쿼리 문자열

**반환값**: 행 또는 오류를 포함하는 `result<database_result>`.

### 통계 및 모니터링

#### get_stats()
```cpp
gateway_stats get_stats() const;
```
게이트웨이 통계를 반환합니다.

**반환값**: 다음을 포함하는 `gateway_stats` 구조체:
- `total_queries`: 처리된 총 쿼리 수
- `successful_queries`: 성공한 쿼리 수
- `failed_queries`: 실패한 쿼리 수
- `cache_hits`: 캐시 적중 수
- `cache_misses`: 캐시 미스 수
- `avg_latency_ms`: 평균 쿼리 지연 시간

## 오류 처리

```cpp
auto result = gateway.execute_query("SELECT * FROM users");
if (result.is_err()) {
    const auto& error = result.error();
    std::cerr << "쿼리 실패: " << error.message << std::endl;
    std::cerr << "오류 코드: " << error.code << std::endl;
}
```

## 스레드 안전성

데이터베이스 게이트웨이는 스레드 안전합니다. 여러 스레드가
외부 동기화 없이 동시에 쿼리를 실행할 수 있습니다.

## 모범 사례

1. **읽기/쓰기 분리 사용** - 읽기 집약적 워크로드에 적합
2. **적절한 캐시 TTL 설정** - 데이터 신선도 요구사항에 따라
3. **캐시 적중률 모니터링** - 캐싱 전략 최적화
4. **연결 제한 설정** - 백엔드 용량에 적합하게
5. **감사 로깅 활성화** - 보안 및 디버깅 목적

## 참고

- [복제 API 레퍼런스](REPLICATION_API_KO.md)
- [클러스터 매니저 문서](CLUSTER_MANAGER_API_KO.md)
