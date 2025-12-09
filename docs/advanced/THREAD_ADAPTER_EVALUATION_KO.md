# Thread Adapter 통합 평가

이 문서는 단일 백그라운드 스레드를 사용하는 모듈에 `thread_adapter`를 통합하는 것에 대한 평가 결과를 기록합니다.

## 관련 이슈

- [#215](https://github.com/kcenon/database_system/issues/215) - performance_monitor.h thread_adapter 평가
- [#214](https://github.com/kcenon/database_system/issues/214) - connection_pool.h thread_adapter 평가  
- [#207](https://github.com/kcenon/database_system/issues/207) - Epic: 외부 시스템을 핵심 모듈에 통합

## 요약

**권장 사항: 단일 장기 실행 스레드를 `thread_adapter`로 마이그레이션하지 마십시오.**

`thread_adapter`는 루프 기반 수명 주기를 가진 단일 영구 스레드를 관리하는 것이 아니라 태스크 풀 실행 패턴을 위해 설계되었습니다.

## 분석 세부 사항

### thread_adapter 설계

`thread_adapter`는 다음을 위해 최적화되었습니다:
- 태스크 풀 실행 (많은 단기 실행 태스크)
- 다중 워커 스레드
- 선택적 우선순위가 있는 태스크 큐잉
- 런타임 백엔드 선택

주요 API 메서드:
```cpp
VoidResult execute(std::function<void()> task);  // Fire-and-forget
std::future<T> submit(F&& f, Args&&... args);    // 결과 얻기
void wait_for_completion();                       // 모든 태스크 대기
```

### 단일 스레드 패턴 (현재)

`performance_monitor.h`와 `connection_pool.h` 모두 유사한 패턴을 사용합니다:
```cpp
// 단일 장기 실행 스레드
std::thread background_thread_;
std::condition_variable cv_;
std::atomic<bool> running_{true};

void background_worker() {
    while (running_) {
        cv_.wait_for(lock, interval, [this]{ return !running_; });
        if (running_) {
            perform_periodic_work();
        }
    }
}
```

### 아키텍처 불일치

| 측면 | 단일 스레드 패턴 | thread_adapter |
|------|------------------|----------------|
| 스레드 수 | 1 | 풀 (N개 워커) |
| 실행 방식 | 연속 루프 | 개별 태스크 |
| 수명 주기 | 한 번 시작, 영원히 루프 | 많은 태스크 제출 |
| 종료 | 조건 변수를 통한 신호 | 태스크 완료 대기 |

### 마이그레이션이 권장되지 않는 이유

1. **잘못된 추상화 수준**: `thread_adapter`는 스레드 수명 주기가 아닌 태스크 풀을 관리합니다

2. **테스트 가능성 이점 없음**: `null_thread_backend`는 태스크를 동기적으로 실행하지만, 루프 기반 스레드는 이 모델에 매핑되지 않습니다

3. **복잡성 추가**: 루프를 외부 상태 관리가 있는 주기적 태스크 제출로 재구성해야 합니다

4. **Sanitizer 처리**: sanitizer 빌드에 대한 현재 컴파일 타임 검사를 복제하기 어렵습니다

## 영향받는 컴포넌트

| 컴포넌트 | 파일 | 결정 |
|----------|------|------|
| Performance Monitor | `database/monitoring/performance_monitor.h` | `std::thread` 유지 |
| Connection Pool | `database/pooling/connection_pool.h` | `std::thread` 유지 |

## thread_adapter 사용 시기

다음 경우에 `thread_adapter`를 사용하십시오:
- 많은 독립적인 태스크를 제출해야 할 때
- 태스크가 단기 실행되고 영구 상태가 필요하지 않을 때
- 런타임 백엔드 선택이 필요할 때 (테스트용 null 백엔드)
- 여러 워커 간의 태스크 병렬 처리가 유익할 때

## std::thread 유지 시기

다음 경우에 `std::thread`를 유지하십시오:
- 루프가 있는 단일 영구 스레드가 있을 때
- 스레드가 sleep/shutdown을 위해 조건 변수를 사용할 때
- 스레드 수명 주기가 소유 클래스에 의해 관리될 때
- 패턴이 잘 이해되고 잘 테스트되었을 때
