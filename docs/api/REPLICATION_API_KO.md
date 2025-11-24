# 복제 매니저 API 레퍼런스

## 개요

복제 매니저는 다양한 복제 모드와 충돌 해결 전략을 지원하여
데이터베이스 노드 간의 자동 데이터 동기화를 제공합니다.

## 빠른 시작

```cpp
#include <database/replication/replication_manager.h>

using namespace database::replication;

// 복제 매니저 생성
replication_manager replication;

// 소스 및 타겟 노드 설정
distributed::node_config source;
source.id = "primary";
source.host = "primary.db.local";
source.port = 5432;
source.role = distributed::node_role::PRIMARY;

distributed::node_config target;
target.id = "replica1";
target.host = "replica1.db.local";
target.port = 5432;
target.role = distributed::node_role::REPLICA;

// 복제 설정
replication_config config;
config.mode = sync_mode::REALTIME;
config.conflict_resolution = conflict_strategy::LAST_WRITE_WINS;
config.batch_size = 100;

// 복제 시작
auto result = replication.start_replication(source, target, config);
if (result.is_ok()) {
    std::cout << "복제가 시작되었습니다" << std::endl;
}
```

## 설정

### replication_config

| 필드 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| mode | sync_mode | REALTIME | 동기화 모드 |
| conflict_resolution | conflict_strategy | LAST_WRITE_WINS | 충돌 해결 전략 |
| batch_size | size_t | 100 | 배치당 변경 사항 수 |
| batch_interval | seconds | 60 | BATCH 모드의 배치 간격 |
| bidirectional | bool | false | 양방향 복제 활성화 |
| tables | vector<table_mapping> | {} | 테이블 매핑 설정 |

### sync_mode (동기화 모드)

- `REALTIME`: 변경 발생 즉시 복제
- `BATCH`: 변경 사항을 배치로 모아 간격을 두고 복제
- `MANUAL`: 명시적으로 트리거할 때만 복제

### conflict_strategy (충돌 해결 전략)

- `LAST_WRITE_WINS`: 가장 최근 변경 사용
- `FIRST_WRITE_WINS`: 첫 번째 변경 사용
- `MANUAL`: 수동 충돌 해결 필요
- `CUSTOM`: 사용자 정의 충돌 해결 콜백 사용

## API 레퍼런스

### 라이프사이클 메서드

#### start_replication()
```cpp
result<void> start_replication(
    const distributed::node_config& source,
    const distributed::node_config& target,
    const replication_config& config
);
```
소스와 타겟 노드 간의 복제를 시작합니다.

**매개변수**:
- `source`: 소스 노드 설정
- `target`: 타겟 노드 설정
- `config`: 복제 설정

**반환값**: 성공 또는 실패를 나타내는 `result<void>`.

#### stop_replication()
```cpp
result<void> stop_replication();
```
활성 복제 프로세스를 중지합니다.

#### is_active()
```cpp
bool is_active() const;
```
복제가 현재 활성 상태인지 반환합니다.

### 제어 메서드

#### pause()
```cpp
result<void> pause();
```
완전히 중지하지 않고 복제를 일시 중지합니다.

**예제**:
```cpp
// 유지보수를 위해 일시 중지
replication.pause();

// ... 유지보수 수행 ...

// 재개
replication.resume();
```

#### resume()
```cpp
result<void> resume();
```
일시 중지된 복제를 재개합니다.

#### trigger_replication()
```cpp
result<void> trigger_replication();
```
수동으로 복제를 트리거합니다 (MANUAL 모드에서만 작동).

### 모니터링 메서드

#### get_replication_lag()
```cpp
std::chrono::milliseconds get_replication_lag() const;
```
현재 복제 지연 시간을 반환합니다.

**예제**:
```cpp
auto lag = replication.get_replication_lag();
if (lag > std::chrono::seconds(5)) {
    std::cerr << "경고: 높은 복제 지연: "
              << lag.count() << "ms" << std::endl;
}
```

#### get_stats()
```cpp
replication_stats get_stats() const;
```
종합적인 복제 통계를 반환합니다.

**반환값**: 다음을 포함하는 `replication_stats` 구조체:
- `events_replicated`: 복제된 총 이벤트 수
- `events_failed`: 실패한 이벤트 수
- `conflicts_resolved`: 해결된 충돌 수
- `current_lag`: 현재 복제 지연
- `avg_lag`: 평균 복제 지연
- `max_lag`: 관측된 최대 지연
- `last_event_time`: 마지막 복제된 이벤트 시간

#### is_healthy()
```cpp
bool is_healthy() const;
```
복제가 정상 상태인지 반환합니다 (활성 상태이고 지연이 낮음).

#### get_pending_event_count()
```cpp
size_t get_pending_event_count() const;
```
복제 대기 중인 이벤트 수를 반환합니다.

## 오류 처리

```cpp
auto result = replication.start_replication(source, target, config);
if (result.is_err()) {
    const auto& error = result.error();

    switch (error.code) {
        case -1:
            std::cerr << "복제가 이미 활성 상태입니다" << std::endl;
            break;
        case -2:
            std::cerr << "복제가 활성 상태가 아닙니다" << std::endl;
            break;
        case -3:
            std::cerr << "작업에 유효하지 않은 모드입니다" << std::endl;
            break;
        case -4:
            std::cerr << "타겟이 초기화되지 않았습니다" << std::endl;
            break;
    }
}
```

## 스레드 안전성

복제 매니저는 스레드 안전합니다. 복제가 활성화된 동안
모든 스레드에서 통계와 상태를 조회할 수 있습니다.

## 모범 사례

1. **낮은 지연 요구사항에 REALTIME 모드 사용**
2. **높은 처리량 시나리오에 BATCH 모드 사용**
3. **복제 지연 모니터링** - 과도한 지연(>5초) 시 알림
4. **적절한 배치 크기 설정** - 쓰기 볼륨에 따라
5. **정기적인 장애 조치 테스트**
6. **선택적 복제를 위한 테이블 매핑 사용**

## 설정 예제

### 고처리량 배치 복제

```cpp
replication_config config;
config.mode = sync_mode::BATCH;
config.batch_size = 1000;
config.batch_interval = std::chrono::seconds(30);
```

### 선택적 테이블 복제

```cpp
replication_config config;
config.mode = sync_mode::REALTIME;

table_mapping users_mapping;
users_mapping.source_table = "users";
users_mapping.target_table = "users_replica";
users_mapping.filter_condition = "status = 'active'";
config.tables.push_back(users_mapping);
```

## 참고

- [게이트웨이 API 레퍼런스](GATEWAY_API_KO.md)
- [클러스터 매니저 문서](CLUSTER_MANAGER_API_KO.md)
