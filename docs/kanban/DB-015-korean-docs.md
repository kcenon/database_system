# DB-015: Update Korean Documentation

**Category**: DOC
**Priority**: LOW
**Status**: TODO
**Est. Duration**: 4-5 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Korean documentation exists but is outdated (ARCHITECTURE_KO.md, API_REFERENCE_KO.md, etc.)
- New distributed features (Gateway, Replication) not documented in Korean
- Some Korean docs have inconsistent terminology
- Missing Korean versions of recent documentation

### Target State
- All major documentation available in Korean
- Consistent Korean technical terminology
- Korean documentation synchronized with English versions
- Korean-specific usage examples where applicable

### Scope
**Files to Update**:
- `docs/ARCHITECTURE_KO.md` - Sync with English version
- `docs/API_REFERENCE_KO.md` - Sync with English version
- `docs/README_KO.md` - Update with new features

**Files to Create**:
- `docs/api/GATEWAY_API_KO.md`
- `docs/api/REPLICATION_API_KO.md`
- `docs/guides/DISTRIBUTED_SETUP_KO.md`
- `docs/performance/TUNING_GUIDE_KO.md`

---

## 2. How to Change

### 2.1 Terminology Standards

Establish consistent Korean translations for technical terms:

| English | Korean | Notes |
|---------|--------|-------|
| Database | 데이터베이스 | Standard usage |
| Connection Pool | 연결 풀 | Direct translation |
| Query Builder | 쿼리 빌더 | Keep English for clarity |
| Replication | 복제 | Standard DB term |
| Gateway | 게이트웨이 | Loan word |
| Failover | 장애 조치 | Or 페일오버 |
| Primary | 프라이머리 / 주 노드 | Context dependent |
| Replica | 레플리카 / 복제본 | Context dependent |
| Cache | 캐시 | Loan word |
| Throughput | 처리량 | Standard translation |
| Latency | 지연 시간 | Standard translation |
| Thread Safety | 스레드 안전성 | Standard translation |

### 2.2 Gateway API Korean Version

```markdown
<!-- docs/api/GATEWAY_API_KO.md -->
# 데이터베이스 게이트웨이 API 참조

> **Language:** [English](GATEWAY_API.md) | **한국어**

## 개요

데이터베이스 게이트웨이는 라우팅, 캐싱, 로드 밸런싱 기능을 갖춘
중앙 집중식 데이터베이스 접근 지점을 제공합니다.

## 빠른 시작

```cpp
#include <database/distributed/database_gateway.h>

using namespace database::distributed;

// 기본 설정으로 게이트웨이 생성
gateway_config config;
config.enable_cache = true;
config.strategy = routing_strategy::ROUND_ROBIN;

database_gateway gateway(config);

// 백엔드 데이터베이스 추가
backend_config backend;
backend.type = database_types::PostgreSQL;
backend.connection_string = "host=localhost;port=5432;database=mydb";
backend.role = backend_role::PRIMARY;

gateway.add_backend("primary", backend);
gateway.initialize();

// 쿼리 실행
auto result = gateway.execute_read("SELECT * FROM users WHERE active = true");
```

## 설정

### gateway_config

| 필드 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| strategy | routing_strategy | ROUND_ROBIN | 쿼리 라우팅 전략 |
| enable_read_write_split | bool | true | 읽기를 레플리카로 라우팅 |
| enable_cache | bool | true | 쿼리 캐싱 활성화 |
| cache_max_entries | size_t | 10000 | 최대 캐시 항목 수 |
| cache_ttl | seconds | 300 | 캐시 TTL (초) |
| max_connections_per_backend | size_t | 20 | 백엔드당 연결 풀 크기 |
| connection_timeout | seconds | 30 | 연결 타임아웃 |

### routing_strategy (라우팅 전략)

- `ROUND_ROBIN`: 백엔드 간 균등 분배
- `LEAST_CONNECTIONS`: 활성 연결이 가장 적은 백엔드로 라우팅
- `RANDOM`: 무작위 백엔드 선택
- `WEIGHTED`: 가중치 기반 분배
- `LATENCY_BASED`: 지연 시간이 가장 낮은 백엔드로 라우팅

## API 참조

### 생명주기 메서드

#### initialize()
```cpp
bool initialize();
```
게이트웨이를 초기화하고 모든 설정된 백엔드에 연결을 설정합니다.

**반환값**: 성공 시 `true`, 실패 시 `false`.

#### shutdown()
```cpp
void shutdown();
```
모든 연결을 종료하고 게이트웨이를 정상적으로 종료합니다.

### 백엔드 관리

#### add_backend()
```cpp
bool add_backend(const std::string& id, const backend_config& config);
```
게이트웨이에 새 데이터베이스 백엔드를 추가합니다.

**매개변수**:
- `id`: 백엔드의 고유 식별자
- `config`: 백엔드 설정

### 쿼리 실행

#### execute_read()
```cpp
database_result execute_read(const std::string& query);
```
읽기 쿼리를 실행합니다. 레플리카로 라우팅될 수 있습니다.

#### execute_write()
```cpp
database_result execute_write(const std::string& query);
```
쓰기 쿼리를 실행합니다. 항상 프라이머리로 라우팅됩니다.

## 스레드 안전성

데이터베이스 게이트웨이는 스레드 안전합니다. 여러 스레드가 외부 동기화 없이
동시에 쿼리를 실행할 수 있습니다.

## 모범 사례

1. 읽기 중심 워크로드에는 **읽기/쓰기 분리** 사용
2. 데이터 신선도 요구사항에 따라 **적절한 캐시 TTL** 설정
3. 캐싱 전략 최적화를 위해 **캐시 적중률 모니터링**
4. 백엔드 용량에 맞는 **연결 제한** 설정
```

### 2.3 Replication API Korean Version

```markdown
<!-- docs/api/REPLICATION_API_KO.md -->
# 복제 관리자 API 참조

> **Language:** [English](REPLICATION_API.md) | **한국어**

## 개요

복제 관리자는 다양한 복제 토폴로지를 지원하며
데이터베이스 노드 간 자동 데이터 동기화를 제공합니다.

## 빠른 시작

```cpp
#include <database/distributed/replication_manager.h>

using namespace database::distributed;

// 복제 설정
replication_config config;
config.topology = topology_type::PRIMARY_REPLICA;
config.mode = replication_mode::ASYNCHRONOUS;

replication_manager replication(config);

// 노드 추가
replication_node primary;
primary.id = "primary";
primary.connection_string = "host=primary;port=5432;database=mydb";
primary.is_primary = true;

replication.add_node(primary);

// 복제 시작
replication.start();
```

## 설정

### replication_mode (복제 모드)

- `SYNCHRONOUS` (동기): 커밋 전 레플리카 확인 대기
- `ASYNCHRONOUS` (비동기): 백그라운드 복제 (최종 일관성)
- `SEMI_SYNCHRONOUS` (반동기): 최소 하나의 레플리카 대기

### topology_type (토폴로지 유형)

- `PRIMARY_REPLICA`: 단일 프라이머리, 다중 레플리카
- `MULTI_PRIMARY`: 충돌 해결이 필요한 다중 프라이머리
- `CHAIN`: 캐스케이딩 복제 (A → B → C)

## 장애 조치

### initiate_failover()
```cpp
bool initiate_failover(const std::string& new_primary_id);
```
새 프라이머리 노드로의 장애 조치를 시작합니다.

**예제**:
```cpp
// 프라이머리 상태 모니터링
if (!replication.is_node_healthy("primary")) {
    // 레플리카를 프라이머리로 승격
    if (replication.initiate_failover("replica1")) {
        std::cout << "장애 조치 성공" << std::endl;
    }
}
```

## 모니터링

### get_replication_lag()
```cpp
uint64_t get_replication_lag(const std::string& replica_id) const;
```
바이트 단위로 복제 지연을 반환합니다.

## 콜백

### on_failover()
```cpp
void on_failover(failover_callback callback);
```
장애 조치 이벤트에 대한 콜백을 등록합니다.

**예제**:
```cpp
replication.on_failover([](const std::string& old_primary,
                           const std::string& new_primary) {
    std::cout << "장애 조치: " << old_primary << " -> " << new_primary << std::endl;
});
```

## 모범 사례

1. 성능을 위해 **비동기 복제**, 내구성을 위해 **동기 복제** 사용
2. **복제 지연 모니터링** 및 과도한 지연 시 알림 설정
3. 정기적으로 **장애 조치 절차 테스트**
4. 쓰기 볼륨에 따른 **적절한 배치 크기** 설정
```

### 2.4 Documentation Structure

```markdown
<!-- docs/README_KO.md additions -->

## 문서 목록

### API 참조
- [API Reference (영문)](API_REFERENCE.md) | [API 참조 (한국어)](API_REFERENCE_KO.md)
- [Gateway API (영문)](api/GATEWAY_API.md) | [게이트웨이 API (한국어)](api/GATEWAY_API_KO.md)
- [Replication API (영문)](api/REPLICATION_API.md) | [복제 API (한국어)](api/REPLICATION_API_KO.md)

### 가이드
- [Architecture (영문)](ARCHITECTURE.md) | [아키텍처 (한국어)](ARCHITECTURE_KO.md)
- [Distributed Setup (영문)](guides/DISTRIBUTED_SETUP.md) | [분산 설정 (한국어)](guides/DISTRIBUTED_SETUP_KO.md)

### 성능
- [Tuning Guide (영문)](performance/TUNING_GUIDE.md) | [튜닝 가이드 (한국어)](performance/TUNING_GUIDE_KO.md)
```

### 2.5 Implementation Steps

1. **Terminology Standardization** (Day 1)
   - Create terminology glossary
   - Review existing Korean docs for consistency
   - Update inconsistent terms

2. **Update Existing Korean Docs** (Day 2)
   - Sync ARCHITECTURE_KO.md
   - Sync API_REFERENCE_KO.md
   - Update README_KO.md

3. **Create New Korean Docs** (Days 3-4)
   - GATEWAY_API_KO.md
   - REPLICATION_API_KO.md
   - DISTRIBUTED_SETUP_KO.md

4. **Review and Polish** (Day 5)
   - Native speaker review
   - Technical accuracy check
   - Cross-link verification

---

## 3. How to Test

### 3.1 Translation Quality

- Native Korean speaker review
- Technical accuracy verification
- Consistency with English version

### 3.2 Documentation Verification

```bash
# Check markdown syntax
markdownlint docs/**/*_KO.md

# Verify links
markdown-link-check docs/**/*_KO.md
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Major docs translated | 100% | Manual count |
| Terminology consistency | 100% | Glossary check |
| Link validity | 100% | Link checker |
| Native review | Passed | Reviewer signoff |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Translation inaccuracy | MEDIUM | Native speaker review |
| Sync with English docs | HIGH | Track English doc changes |
| Inconsistent terminology | LOW | Maintain glossary |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-010](DB-010-api-docs.md) (API Documentation)
  - [DB-013](DB-013-tuning-guide.md) (Tuning Guide)

---

## 6. Notes

- Maintain parallel structure with English docs
- Use consistent file naming convention (*_KO.md)
- Consider automated translation quality checks
- Update Korean docs when English docs change

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
