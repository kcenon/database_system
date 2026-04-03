---
doc_id: "DBS-PROJ-007"
doc_title: "Thread Adapter Integration Evaluation"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "PROJ"
---

# Thread Adapter Integration Evaluation

This document records the evaluation outcome for integrating `thread_adapter` into modules that use single background threads.

## Related Issues

- [#215](https://github.com/kcenon/database_system/issues/215) - performance_monitor.h thread_adapter evaluation
- [#214](https://github.com/kcenon/database_system/issues/214) - connection_pool.h thread_adapter evaluation  
- [#207](https://github.com/kcenon/database_system/issues/207) - Epic: Integrate external systems into core modules

## Summary

**Recommendation: Do NOT migrate single long-running threads to `thread_adapter`.**

The `thread_adapter` is designed for task-pool execution patterns, not for managing single persistent threads with loop-based lifecycles.

## Analysis Details

### thread_adapter Design

The `thread_adapter` is optimized for:
- Task pool execution (many short-lived tasks)
- Multiple worker threads
- Task queuing with optional priority
- Runtime backend selection

Key API methods:
```cpp
VoidResult execute(std::function<void()> task);  // Fire-and-forget
std::future<T> submit(F&& f, Args&&... args);    // Get result
void wait_for_completion();                       // Wait for all tasks
```

### Single Thread Pattern (Current)

Both `performance_monitor.h` and `connection_pool.h` use a similar pattern:
```cpp
// Single long-running thread
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

### Architectural Mismatch

| Aspect | Single Thread Pattern | thread_adapter |
|--------|----------------------|----------------|
| Thread count | 1 | Pool (N workers) |
| Execution | Continuous loop | Discrete tasks |
| Lifecycle | Start once, loop forever | Submit many tasks |
| Shutdown | Signal via condition variable | Wait for task completion |

### Why Migration Is Not Recommended

1. **Wrong abstraction level**: `thread_adapter` manages task pools, not thread lifecycles

2. **No testability benefit**: The `null_thread_backend` runs tasks synchronously, but loop-based threads don't map to this model

3. **Added complexity**: Would require restructuring the loop into periodic task submissions with external state management

4. **Sanitizer handling**: Current compile-time checks for sanitizer builds would be harder to replicate

## Affected Components

| Component | File | Decision |
|-----------|------|----------|
| Performance Monitor | `database/monitoring/performance_monitor.h` | Keep `std::thread` |
| Connection Pool | `database/pooling/connection_pool.h` | Keep `std::thread` |

## When to Use thread_adapter

Use `thread_adapter` when:
- You need to submit many independent tasks
- Tasks are short-lived and don't require persistent state
- You want runtime backend selection (null backend for testing)
- Task parallelism across multiple workers is beneficial

## When to Keep std::thread

Keep `std::thread` when:
- You have a single persistent thread with a loop
- The thread uses condition variable for sleep/shutdown
- Thread lifecycle is managed by the owning class
- The pattern is well-understood and well-tested
