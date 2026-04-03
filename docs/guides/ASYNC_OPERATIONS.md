---
doc_id: "DBS-GUID-010"
doc_title: "Async Database Operations Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Async Database Operations Guide

> **Language:** **English** | [한국어](ASYNC_OPERATIONS.kr.md)

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
  - [Async Execution Model](#async-execution-model)
  - [thread\_system Integration](#thread_system-integration)
- [async\_executor](#async_executor)
  - [Construction](#construction)
  - [Task Submission](#task-submission)
  - [Lifecycle Management](#lifecycle-management)
  - [Performance Characteristics](#performance-characteristics)
- [async\_result\<T\>](#async_resultt)
  - [Blocking Operations](#blocking-operations)
  - [Non-Blocking Operations](#non-blocking-operations)
  - [Callback Support](#callback-support)
  - [Helper Functions](#helper-functions)
- [async\_database](#async_database)
  - [Query Operations](#query-operations)
  - [Batch Operations](#batch-operations)
  - [Async Transaction Operations](#async-transaction-operations)
  - [Async Connection Management](#async-connection-management)
- [C++20 Coroutine Support](#c20-coroutine-support)
  - [database\_awaitable\<T\>](#database_awaitablet)
  - [Coroutine Helpers](#coroutine-helpers)
- [Stream Processing](#stream-processing)
  - [Stream Types](#stream-types)
  - [Stream Events](#stream-events)
  - [Event Handlers and Filters](#event-handlers-and-filters)
- [Distributed Transactions](#distributed-transactions)
  - [transaction\_coordinator](#transaction_coordinator)
  - [Two-Phase Commit](#two-phase-commit)
  - [Saga Pattern](#saga-pattern)
- [Thread Safety](#thread-safety)
- [Error Handling](#error-handling)
  - [Future-Based Errors](#future-based-errors)
  - [Callback-Based Errors](#callback-based-errors)
  - [Timeout Handling](#timeout-handling)
- [Performance Considerations](#performance-considerations)
  - [When to Use Async vs Sync](#when-to-use-async-vs-sync)
  - [Connection Pool Sizing](#connection-pool-sizing)
  - [Batching Strategies](#batching-strategies)
  - [Backpressure Handling](#backpressure-handling)
- [Examples](#examples)
  - [Example 1: Basic Async Queries](#example-1-basic-async-queries)
  - [Example 2: Stream Processing with Filters](#example-2-stream-processing-with-filters)
  - [Example 3: Distributed Transaction with Saga](#example-3-distributed-transaction-with-saga)
  - [Example 4: Coroutine-Based Workflow](#example-4-coroutine-based-workflow)
- [Limitations and Known Issues](#limitations-and-known-issues)
- [Related Documentation](#related-documentation)

---

## Overview

The async operations module (`database/async/async_operations.h`) provides non-blocking database access through multiple asynchronous patterns:

- **Future/Promise** — `async_result<T>` for deferred result retrieval
- **Callbacks** — `then()` / `on_error()` for event-driven handling
- **C++20 Coroutines** — `database_awaitable<T>` for sequential-looking async code
- **Stream Processing** — `stream_processor` for real-time database event subscriptions
- **Distributed Transactions** — Two-Phase Commit and Saga pattern support

**Source files:**

| File | Purpose |
|------|---------|
| `database/async/async_operations.h` | All async classes: executor, result, database, streams, transactions |
| `database/adapters/thread_pool_adapter.h` | Adapter for thread_system / std::thread fallback |
| `database/core/concepts.h` | C++20 concepts (`SubmittableTask`, `StreamEventHandler`, etc.) |

**Namespace:** `database::async`

---

## Architecture

### Async Execution Model

```text
┌─────────────────────────────────────────────────────────────┐
│                    Application Code                          │
│   async_database  │  stream_processor  │  transaction_coord  │
├─────────────────────────────────────────────────────────────┤
│                    async_result<T>                            │
│         Blocking  │  Non-Blocking  │  Callbacks              │
├─────────────────────────────────────────────────────────────┤
│                    async_executor                             │
│         submit()  │  shutdown()  │  wait_for_completion()    │
├───────────────────────────┬─────────────────────────────────┤
│  USE_THREAD_SYSTEM        │  Fallback (std::thread)         │
│  kcenon::thread::         │  std::thread pool               │
│  thread_pool              │  std::mutex +                   │
│  (1.16M+ jobs/s)          │  std::condition_variable        │
└───────────────────────────┴─────────────────────────────────┘
```

### thread_system Integration

The module uses compile-time adapter pattern via `thread_pool_adapter.h`:

| Macro | Backend | Performance |
|-------|---------|-------------|
| `USE_THREAD_SYSTEM` defined | `kcenon::thread::thread_pool` | 1.16M+ jobs/s, 77ns latency |
| Not defined (fallback) | `std::thread` pool with mutex | ~50K jobs/s, 2-5us latency |

The `constexpr bool using_thread_system` flag enables runtime detection:

```cpp
async_executor executor(4);
if (executor.is_using_thread_system()) {
    // High-performance thread_system backend
} else {
    // Standard library fallback
}
```

**Type aliases** defined by `thread_pool_adapter.h`:

| Alias | USE_THREAD_SYSTEM | Fallback |
|-------|-------------------|----------|
| `thread_pool_type` | `kcenon::thread::thread_pool` | `fallback_thread_pool` |
| `thread_context_type` | `kcenon::thread::thread_context` | `fallback_context` (no-op) |
| `job_type` | `kcenon::thread::job` | `fallback_job` |
| `result_type<T>` | `kcenon::common::Result<T>` | Inline `result_type<T>` |
| `result_void_type` | `kcenon::common::VoidResult` | Inline `result_void_type` |

---

## async_executor

The core execution engine for all async operations.

**Header:** `database/async/async_operations.h`

### Construction

```cpp
#include "database/async/async_operations.h"

using namespace database::async;

// Default: hardware_concurrency threads
async_executor executor;

// Custom thread count
async_executor executor(8);

// With thread context (for logging/monitoring when USE_THREAD_SYSTEM)
thread_context_type context;
async_executor executor(8, context);
```

The executor is **non-copyable and non-moveable**. It starts worker threads immediately on construction.

### Task Submission

The `submit()` method accepts any callable constrained by the `SubmittableTask` C++20 concept:

```cpp
// Submit a lambda returning int
auto future = executor.submit([]() -> int {
    return compute_something();
});
int result = future.get();

// Submit with arguments
auto future = executor.submit([](int a, int b) { return a + b; }, 10, 20);

// Submit void-returning task (fire-and-forget with future)
auto future = executor.submit([]() {
    cleanup_resources();
});
future.wait(); // Optional: wait for completion
```

**Concept constraint:**

```cpp
template<typename F, typename... Args>
    requires concepts::SubmittableTask<F, Args...>
auto submit(F&& func, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>;
```

`SubmittableTask` requires that `F` is invocable with `Args...` and is move-constructible.

### Lifecycle Management

```cpp
async_executor executor(4);

// Check pending task count
size_t pending = executor.pending_tasks();

// Get worker thread count
size_t workers = executor.thread_count();

// Wait for all pending tasks to finish (busy-wait with 10ms polling)
executor.wait_for_completion();

// Graceful shutdown (waits for in-flight tasks, rejects new submissions)
executor.shutdown();
// Destructor also calls shutdown() automatically
```

### Performance Characteristics

| Metric | thread_system | std::thread fallback |
|--------|---------------|----------------------|
| Throughput | 1.16M+ jobs/s | ~50K jobs/s |
| Scheduling latency | 77ns | 2-5us |
| Scalability | Linear to HW concurrency | Limited by lock contention |
| Job queue | Adaptive (mutex / lock-free) | `std::queue` with `std::mutex` |

---

## async_result\<T\>

Template class wrapping `std::future<T>` with callback support.

### Blocking Operations

```cpp
async_result<int> result = executor.submit([]() { return 42; });

// Block until result is available
int value = result.get();

// Block with timeout
try {
    int value = result.get_for(std::chrono::milliseconds(5000));
} catch (const std::runtime_error& e) {
    // Timeout exceeded
}
```

> **Important:** `get()` and `get_for()` consume the future. They can only be called once.

### Non-Blocking Operations

```cpp
async_result<int> result = /* ... */;

// Check if result is ready without blocking
if (result.is_ready()) {
    int value = result.get();
}

// Wait with status check
auto status = result.wait_for(std::chrono::milliseconds(100));
if (status == std::future_status::ready) {
    int value = result.get();
} else if (status == std::future_status::timeout) {
    // Still running
}
```

### Callback Support

Callbacks provide event-driven result handling. Registration methods are thread-safe.

**With C++20 concepts:**

```cpp
async_result<database::core::database_result> result = db.select_async("SELECT ...");

// Success callback — constrained by VoidCallable<T> concept
result.then([](database::core::database_result rows) {
    for (const auto& row : rows) {
        process_row(row);
    }
});

// Error callback — constrained by ErrorHandler concept
result.on_error([](const std::exception& e) {
    std::cerr << "Query failed: " << e.what() << "\n";
});
```

**Legacy overloads** (for backward compatibility):

```cpp
result.then(std::function<void(database::core::database_result)>([](auto rows) {
    // handle result
}));

result.on_error(std::function<void(const std::exception&)>([](auto& e) {
    // handle error
}));
```

### Helper Functions

```cpp
// Create an already-resolved result
auto ready = make_ready_result<int>(42);
int value = ready.get(); // Returns immediately

// Create an already-failed result
auto failed = make_error_result<int>(std::runtime_error("failed"));
try {
    failed.get(); // Throws immediately
} catch (const std::runtime_error& e) {
    // "failed"
}
```

---

## async_database

Wraps a `database_backend` with an `async_executor` to provide fully async database operations.

### Query Operations

```cpp
auto db_backend = /* std::shared_ptr<core::database_backend> */;
auto executor = std::make_shared<async_executor>(4);
async_database db(db_backend, executor);

// Async execute (DDL, INSERT, UPDATE, DELETE) — returns async_result<bool>
auto exec_result = db.execute_async("INSERT INTO users (name) VALUES ('Alice')");
bool success = exec_result.get();

// Async select — returns async_result<core::database_result>
auto select_result = db.select_async("SELECT * FROM users WHERE active = true");
auto rows = select_result.get();
```

### Batch Operations

Execute multiple queries in a single async operation:

```cpp
// Batch execute — returns async_result<std::vector<bool>>
std::vector<std::string> inserts = {
    "INSERT INTO logs (msg) VALUES ('event1')",
    "INSERT INTO logs (msg) VALUES ('event2')",
    "INSERT INTO logs (msg) VALUES ('event3')"
};
auto batch_result = db.execute_batch_async(inserts);
auto results = batch_result.get(); // vector<bool>

// Batch select — returns async_result<std::vector<core::database_result>>
std::vector<std::string> queries = {
    "SELECT COUNT(*) FROM users",
    "SELECT COUNT(*) FROM orders",
    "SELECT COUNT(*) FROM products"
};
auto batch_select = db.select_batch_async(queries);
auto all_results = batch_select.get(); // vector<database_result>
```

### Async Transaction Operations

```cpp
// Begin transaction asynchronously
auto begin = db.begin_transaction_async();
if (begin.get()) {
    auto exec1 = db.execute_async("INSERT INTO orders ...");
    auto exec2 = db.execute_async("UPDATE inventory ...");

    if (exec1.get() && exec2.get()) {
        auto commit = db.commit_transaction_async();
        commit.get();
    } else {
        auto rollback = db.rollback_transaction_async();
        rollback.get();
    }
}
```

### Async Connection Management

```cpp
// Connect asynchronously
auto connect = db.connect_async("host=localhost dbname=mydb");
if (connect.get()) {
    // Connected — execute queries
}

// Disconnect asynchronously
auto disconnect = db.disconnect_async();
disconnect.get();
```

---

## C++20 Coroutine Support

Available when `HAS_COROUTINES` is defined at compile time.

### database_awaitable\<T\>

A coroutine return type that supports `co_await` for database operations:

```cpp
#ifdef HAS_COROUTINES
#include "database/async/async_operations.h"

using namespace database::async;

// Coroutine-based database operations
database_awaitable<bool> insert_user(async_database& db, const std::string& name) {
    auto result = co_await db.execute_coro(
        "INSERT INTO users (name) VALUES ('" + name + "')");
    co_return result;
}

database_awaitable<core::database_result> get_active_users(async_database& db) {
    auto result = co_await db.select_coro("SELECT * FROM users WHERE active = true");
    co_return result;
}
#endif
```

**Coroutine methods on `async_database`:**

| Method | Returns | Description |
|--------|---------|-------------|
| `execute_coro(query)` | `database_awaitable<bool>` | Async execute via coroutine |
| `select_coro(query)` | `database_awaitable<core::database_result>` | Async select via coroutine |

**`database_awaitable<T>` characteristics:**
- Move-only (non-copyable)
- `await_ready()` returns `true` if coroutine is already done
- `await_resume()` rethrows any captured exception
- Destroys coroutine handle in destructor

### Coroutine Helpers

```cpp
#ifdef HAS_COROUTINES
// Wait for all awaitables to complete
std::vector<database_awaitable<bool>> tasks;
tasks.push_back(db.execute_coro("INSERT ..."));
tasks.push_back(db.execute_coro("UPDATE ..."));
auto all_results = co_await when_all(std::move(tasks));
// all_results is std::vector<bool>

// Wait for first completion (returns first result)
auto first = co_await when_any(std::move(tasks));
#endif
```

---

## Stream Processing

The `stream_processor` provides real-time database event subscriptions.

### Stream Types

```cpp
enum class stream_type {
    postgresql_notify,       // PostgreSQL LISTEN/NOTIFY
    mongodb_change_stream,   // MongoDB Change Streams
    redis_pubsub,           // Redis Pub/Sub
    custom                  // Custom event source
};
```

### Stream Events

```cpp
struct stream_event {
    stream_type type;          // Event source type
    std::string channel;       // Channel/topic name
    std::string payload;       // Event payload data
    std::chrono::system_clock::time_point timestamp;  // Event timestamp
    std::unordered_map<std::string, std::string> metadata;  // Additional metadata
};
```

### Event Handlers and Filters

```cpp
auto db_backend = /* std::shared_ptr<core::database_backend> */;
stream_processor processor(db_backend);

// Start listening on a channel
processor.start_stream(stream_processor::stream_type::postgresql_notify, "user_events");

// Register channel-specific handler (C++20 concept constrained)
processor.register_event_handler("user_events", [](const stream_processor::stream_event& event) {
    std::cout << "Event on " << event.channel << ": " << event.payload << "\n";
});

// Register global handler (receives events from ALL channels)
processor.register_global_handler([](const stream_processor::stream_event& event) {
    log_event(event);
});

// Add event filter (only matching events reach the handler)
processor.add_event_filter("user_events", [](const stream_processor::stream_event& event) {
    return event.payload.find("INSERT") != std::string::npos;
});

// Stop specific stream
processor.stop_stream("user_events");

// Stop all streams
processor.stop_all_streams();
```

**C++20 concept constraints:**

| Method | Concept | Requirement |
|--------|---------|-------------|
| `register_event_handler()` | `StreamEventHandler<stream_event>` | `void(const stream_event&)` |
| `register_global_handler()` | `StreamEventHandler<stream_event>` | `void(const stream_event&)` |
| `add_event_filter()` | `StreamEventFilter<stream_event>` | `bool(const stream_event&)` |

All handler/filter methods also have legacy `std::function` overloads for backward compatibility.

---

## Distributed Transactions

### transaction_coordinator

Manages distributed transactions across multiple database backends.

**Transaction states:**

```text
active → preparing → prepared → committing → committed
   │                     │
   └── aborting ← ───────┘
          │
          ▼
       aborted
```

```cpp
enum class transaction_state {
    active,      // Transaction started
    preparing,   // Prepare phase in progress
    prepared,    // All participants prepared
    committing,  // Commit phase in progress
    committed,   // Transaction committed
    aborting,    // Abort in progress
    aborted      // Transaction aborted
};
```

**Access via dependency injection:**

```cpp
auto context = std::make_shared<database_context>();
auto txn_coord = context->get_transaction_coordinator();
```

### Two-Phase Commit

```cpp
// Participants: multiple database backends
std::vector<std::shared_ptr<core::database_backend>> participants = {
    postgres_backend, sqlite_backend
};

// Begin distributed transaction
std::string txn_id = txn_coord.begin_distributed_transaction(participants);

// Phase 1: Prepare all participants
auto prepare = txn_coord.prepare_phase(txn_id);
if (prepare.get()) {
    // Phase 2: Commit all participants
    auto commit = txn_coord.commit_phase(txn_id);
    commit.get();
} else {
    // Abort if any participant cannot prepare
    auto rollback = txn_coord.rollback_distributed_transaction(txn_id);
    rollback.get();
}

// Or use the combined method:
auto result = txn_coord.commit_distributed_transaction(txn_id);
if (!result.get()) {
    // Commit failed — transaction coordinator handles rollback
}
```

**Query active transactions:**

```cpp
auto active = txn_coord.get_active_transactions();
for (const auto& txn : active) {
    std::cout << "Transaction " << txn.transaction_id
              << " state: " << static_cast<int>(txn.state)
              << " participants: " << txn.participants.size() << "\n";
}

// Recover incomplete transactions after crash
txn_coord.recover_transactions();
```

### Saga Pattern

For long-running transactions where each step has a compensating action:

```cpp
auto saga = txn_coord.create_saga();

// Add steps with forward action and compensation (rollback)
// Constrained by TransactionAction and CompensationAction concepts
saga.add_step(
    []() -> async_result<bool> {
        // Forward: Create order
        return make_ready_result(true);
    },
    []() -> async_result<bool> {
        // Compensate: Cancel order
        return make_ready_result(true);
    }
);

saga.add_step(
    []() -> async_result<bool> {
        // Forward: Reserve inventory
        return make_ready_result(true);
    },
    []() -> async_result<bool> {
        // Compensate: Release inventory
        return make_ready_result(true);
    }
);

saga.add_step(
    []() -> async_result<bool> {
        // Forward: Charge payment
        return make_ready_result(true);
    },
    []() -> async_result<bool> {
        // Compensate: Refund payment
        return make_ready_result(true);
    }
);

// Execute saga — if any step fails, previous compensations run in reverse order
auto result = saga.execute();
if (result.get()) {
    // All steps completed successfully
} else {
    // A step failed — compensations have been applied
}
```

---

## Thread Safety

| Class | Thread Safety | Details |
|-------|-------------|---------|
| `async_executor` | All methods thread-safe | `submit()`, `shutdown()`, `pending_tasks()` safe from multiple threads |
| `async_result<T>` | Callback registration thread-safe | `then()` and `on_error()` are protected by `callback_mutex_`. `get()` and `get_for()` should be called only once. |
| `async_database` | Thread-safe via executor | Operations are dispatched to `async_executor` which handles synchronization |
| `stream_processor` | All public methods thread-safe | `threads_mutex_` protects stream threads, `handlers_mutex_` protects handler/filter containers |
| `transaction_coordinator` | All public methods thread-safe | `transactions_mutex_` protects all transaction state |
| `saga_builder` | Not thread-safe | Build saga from a single thread; `execute()` result is thread-safe |
| `database_awaitable<T>` | Not thread-safe | Single-coroutine use only |

---

## Error Handling

### Future-Based Errors

Exceptions thrown in async tasks propagate through `std::future`:

```cpp
auto result = executor.submit([]() -> int {
    throw std::runtime_error("database connection lost");
    return 0;
});

try {
    int value = result.get(); // Rethrows the exception
} catch (const std::runtime_error& e) {
    std::cerr << "Async error: " << e.what() << "\n";
}
```

### Callback-Based Errors

Register an error handler to catch exceptions without blocking:

```cpp
auto result = db.select_async("SELECT ...");

result.then([](auto rows) {
    // Success path
});

result.on_error([](const std::exception& e) {
    // Error path — called if the async operation throws
    log_error("Query failed", e.what());
});
```

### Timeout Handling

```cpp
auto result = db.select_async("SELECT * FROM large_table");

try {
    auto rows = result.get_for(std::chrono::milliseconds(5000));
} catch (const std::runtime_error& e) {
    // Timeout — query may still be running in background
    std::cerr << "Query timed out after 5 seconds\n";
}

// Alternative: non-blocking check
auto status = result.wait_for(std::chrono::milliseconds(100));
if (status == std::future_status::timeout) {
    // Not ready yet — decide whether to wait longer or cancel
}
```

---

## Performance Considerations

### When to Use Async vs Sync

| Scenario | Recommendation | Reason |
|----------|---------------|--------|
| Single query, need result immediately | **Sync** | Async overhead unnecessary |
| Multiple independent queries | **Async** | Parallel execution reduces total latency |
| Long-running queries on UI thread | **Async** | Prevents blocking |
| High-throughput batch inserts | **Async batch** | `execute_batch_async()` amortizes overhead |
| Real-time event subscriptions | **Stream** | `stream_processor` with dedicated threads |
| Low-latency critical path | **Sync** (direct) | Avoids task queue scheduling overhead |

### Connection Pool Sizing

For async workloads, the connection pool should accommodate concurrent operations:

```text
Recommended max_connections >= async_executor thread_count + sync_connections_needed
```

Example: 8-thread executor + 2 sync connections = pool of at least 10.

### Batching Strategies

```cpp
// Individual async (N round-trips)
for (const auto& query : queries) {
    futures.push_back(db.execute_async(query));
}

// Batch async (1 submission, internal parallelism)
auto batch = db.execute_batch_async(queries);  // Preferred for many small queries
```

### Backpressure Handling

Monitor the executor's pending task queue to prevent memory exhaustion:

```cpp
async_executor executor(4);

// Check queue size before submitting
if (executor.pending_tasks() > 10000) {
    // Apply backpressure: wait for some tasks to complete
    executor.wait_for_completion();
}

executor.submit(/* new task */);
```

---

## Examples

### Example 1: Basic Async Queries

```cpp
#include "database/async/async_operations.h"

using namespace database::async;

int main() {
    // Create executor and async database
    auto executor = std::make_shared<async_executor>(4);
    auto backend = /* obtain database_backend */;
    async_database db(backend, executor);

    // Connect
    auto conn = db.connect_async("host=localhost dbname=mydb");
    if (!conn.get()) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    // Execute multiple queries in parallel
    auto insert1 = db.execute_async("INSERT INTO users (name) VALUES ('Alice')");
    auto insert2 = db.execute_async("INSERT INTO users (name) VALUES ('Bob')");
    auto select = db.select_async("SELECT COUNT(*) FROM users");

    // Collect results
    bool ok1 = insert1.get();
    bool ok2 = insert2.get();
    auto rows = select.get();

    std::cout << "Insert 1: " << ok1 << ", Insert 2: " << ok2 << "\n";

    // Disconnect
    db.disconnect_async().get();
    return 0;
}
```

### Example 2: Stream Processing with Filters

```cpp
#include "database/async/async_operations.h"

using namespace database::async;

int main() {
    auto backend = /* obtain database_backend */;
    stream_processor processor(backend);

    // Listen for PostgreSQL NOTIFY events
    processor.start_stream(
        stream_processor::stream_type::postgresql_notify,
        "order_events");

    // Filter: only INSERT events
    processor.add_event_filter("order_events",
        [](const stream_processor::stream_event& event) {
            return event.payload.find("INSERT") != std::string::npos;
        });

    // Handle filtered events
    processor.register_event_handler("order_events",
        [](const stream_processor::stream_event& event) {
            std::cout << "[" << event.channel << "] "
                      << event.payload << "\n";
        });

    // Global handler for audit logging
    processor.register_global_handler(
        [](const stream_processor::stream_event& event) {
            audit_log(event.channel, event.payload, event.timestamp);
        });

    // Run until shutdown signal...
    std::this_thread::sleep_for(std::chrono::hours(1));

    processor.stop_all_streams();
    return 0;
}
```

### Example 3: Distributed Transaction with Saga

```cpp
#include "database/async/async_operations.h"

using namespace database::async;

int main() {
    auto context = std::make_shared<database_context>();
    auto txn_coord = context->get_transaction_coordinator();

    auto saga = txn_coord.create_saga();

    // Step 1: Create order
    saga.add_step(
        [&]() -> async_result<bool> {
            return db.execute_async("INSERT INTO orders (user_id, total) VALUES (1, 99.99)");
        },
        [&]() -> async_result<bool> {
            return db.execute_async("DELETE FROM orders WHERE user_id = 1 ORDER BY id DESC LIMIT 1");
        }
    );

    // Step 2: Reserve inventory
    saga.add_step(
        [&]() -> async_result<bool> {
            return db.execute_async("UPDATE inventory SET reserved = reserved + 1 WHERE item_id = 42");
        },
        [&]() -> async_result<bool> {
            return db.execute_async("UPDATE inventory SET reserved = reserved - 1 WHERE item_id = 42");
        }
    );

    // Step 3: Charge payment
    saga.add_step(
        [&]() -> async_result<bool> {
            return db.execute_async("INSERT INTO payments (order_id, amount) VALUES (LASTVAL(), 99.99)");
        },
        [&]() -> async_result<bool> {
            return db.execute_async("UPDATE payments SET status = 'refunded' WHERE order_id = LASTVAL()");
        }
    );

    auto result = saga.execute();
    if (result.get()) {
        std::cout << "Order placed successfully\n";
    } else {
        std::cout << "Order failed — compensations applied\n";
    }

    return 0;
}
```

### Example 4: Coroutine-Based Workflow

```cpp
#ifdef HAS_COROUTINES
#include "database/async/async_operations.h"

using namespace database::async;

database_awaitable<bool> process_new_user(async_database& db, const std::string& name) {
    // Sequential-looking code that runs asynchronously
    auto created = co_await db.execute_coro(
        "INSERT INTO users (name) VALUES ('" + name + "')");

    if (!created) {
        co_return false;
    }

    auto notified = co_await db.execute_coro(
        "INSERT INTO notifications (msg) VALUES ('New user: " + name + "')");

    co_return notified;
}

database_awaitable<std::vector<bool>> onboard_users(async_database& db) {
    std::vector<database_awaitable<bool>> tasks;
    tasks.push_back(process_new_user(db, "Alice"));
    tasks.push_back(process_new_user(db, "Bob"));
    tasks.push_back(process_new_user(db, "Charlie"));

    co_return co_await when_all(std::move(tasks));
}
#endif
```

---

## Limitations and Known Issues

| Area | Limitation | Details |
|------|-----------|---------|
| `async_result<T>` | Single-consumer | `get()` / `get_for()` can only be called once (consumes the future) |
| Coroutine support | Compile-time only | Requires `HAS_COROUTINES` macro; no runtime fallback |
| `stream_processor` | Polling-based | Stream threads use polling; latency depends on implementation |
| `transaction_coordinator` | Needs executor | Requires `async_executor` for 2PC and saga execution |
| `when_any` | Simplified | Current implementation returns first awaitable, not first completed |
| Cancellation | Not supported | No cancellation mechanism for in-flight async operations |
| `async_executor` fallback | Busy-wait | `wait_for_completion()` uses 10ms polling loop, not condition-variable wait |

---

## Related Documentation

- [Architecture Overview](../ARCHITECTURE.md) — System-wide architecture and design patterns
- [API Reference](../API_REFERENCE.md) — Complete API reference including async operations
- [Unified System Guide](UNIFIED_SYSTEM.md) — Unified database system with integrated async support
- [C++20 Concepts](../API_REFERENCE.md#c20-concepts) — Full concept reference including `SubmittableTask`, `StreamEventHandler`

---

*Last Updated: 2025-10-20*
