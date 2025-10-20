# Database System - Improvement Plan

> **Language:** **English** | [한국어](IMPROVEMENTS_KO.md)

## Current Status

**Version:** 1.0.0
**Last Review:** 2025-01-20
**Overall Score:** 3.0/5

### Critical Issues

## 1. Connection Pool - Potential Connection Leak

**Location:** `database/connection_pool.h:262`

**Current Issue:**
```cpp
Result<std::shared_ptr<connection_wrapper>> acquire_connection() override {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    // Wait for available connection
    if (!pool_condition_.wait_for(lock, config_.acquire_timeout, [this] {
        return !available_connections_.empty() ||
               (active_count_ < config_.max_connections && !shutdown_requested_);
    })) {
        failed_acquisitions_.fetch_add(1, std::memory_order_relaxed);
        return error_info{-501, "Connection acquisition timeout", "connection_pool"};
    }

    if (shutdown_requested_) {
        return error_info{-500, "Pool is shutting down", "connection_pool"};
    }

    // Try to get from pool
    if (!available_connections_.empty()) {
        auto conn = available_connections_.front();
        available_connections_.pop();
        active_count_.fetch_add(1, std::memory_order_relaxed);
        return conn;
    }

    // Create new connection
    auto db_conn = create_connection();
    if (!db_conn) {
        return error_info{-502, "Failed to create connection", "connection_pool"};
    }

    auto wrapper = std::make_shared<connection_wrapper>(std::move(db_conn));
    active_count_.fetch_add(1, std::memory_order_relaxed);
    total_created_.fetch_add(1, std::memory_order_relaxed);

    return wrapper;  // ❌ If exception after this, connection is leaked!
}
```

**Problem:**
- If caller's code throws after `acquire_connection()` but before `release_connection()`, connection is leaked
- No RAII wrapper provided

**Solution:**
```cpp
// Add RAII wrapper
class scoped_connection {
public:
    scoped_connection(std::shared_ptr<connection_pool_base> pool,
                     std::shared_ptr<connection_wrapper> conn)
        : pool_(std::move(pool)), conn_(std::move(conn)) {}

    ~scoped_connection() {
        if (conn_ && pool_) {
            pool_->release_connection(std::move(conn_));
        }
    }

    // Move-only
    scoped_connection(scoped_connection&&) = default;
    scoped_connection& operator=(scoped_connection&&) = default;
    scoped_connection(const scoped_connection&) = delete;
    scoped_connection& operator=(const scoped_connection&) = delete;

    database_base* get() const { return conn_->get(); }
    database_base* operator->() const { return conn_->get(); }
    database_base& operator*() const { return *conn_->get(); }

    // Explicit release for special cases
    void release() { conn_.reset(); }

private:
    std::shared_ptr<connection_pool_base> pool_;
    std::shared_ptr<connection_wrapper> conn_;
};

// New API
class connection_pool : public connection_pool_base {
public:
    Result<scoped_connection> acquire() {
        auto result = acquire_connection();
        if (!result) {
            return result.error();
        }
        return scoped_connection(shared_from_this(), result.value());
    }
};

// Usage:
auto conn_result = pool->acquire();
if (!conn_result) {
    return conn_result.error();
}
auto conn = std::move(conn_result.value());
conn->execute_query("SELECT...");  // Auto-released on scope exit
```

**Priority:** P0
**Effort:** 1-2 days

---

## 2. Singleton Pattern in database_manager

**Location:** `database/database_manager.h:229-253`

**Problem:**
- Global state makes testing difficult
- Hidden dependencies
- Cannot have multiple database managers

**Solution:**
```cpp
// Remove singleton, use DI
class application {
    std::shared_ptr<database_manager> db_manager_;

public:
    application(std::shared_ptr<database_manager> db_mgr)
        : db_manager_(std::move(db_mgr)) {}

    void run() {
        db_manager_->connect("...");
        // ...
    }
};

// main.cpp
int main() {
    auto db_mgr = std::make_shared<database_manager>();
    auto app = std::make_shared<application>(db_mgr);
    app->run();
}

// Testing:
TEST(DatabaseTest, CanMockDatabaseManager) {
    auto mock_db = std::make_shared<mock_database_manager>();
    application app(mock_db);
    // Test with mock
}
```

**Priority:** P1
**Effort:** 2-3 days

---

## High Priority Improvements

### 3. Add Prepared Statement Support

```cpp
class prepared_statement {
public:
    template<typename... Args>
    result<database_result> execute(Args&&... args) {
        bind_parameters(std::forward<Args>(args)...);
        return execute_impl();
    }

private:
    template<typename T, typename... Rest>
    void bind_parameters(T&& first, Rest&&... rest) {
        bind_parameter(param_index_++, std::forward<T>(first));
        if constexpr (sizeof...(Rest) > 0) {
            bind_parameters(std::forward<Rest>(rest)...);
        }
    }

    size_t param_index_ = 0;
};

// Usage:
auto stmt = db->prepare("SELECT * FROM users WHERE id = ? AND active = ?");
auto result = stmt->execute(user_id, true);
```

**Priority:** P2
**Effort:** 5-7 days

---

### 4. Add Transaction Support

```cpp
class transaction {
public:
    explicit transaction(database_base* db) : db_(db) {
        db_->execute_query("BEGIN TRANSACTION");
    }

    ~transaction() {
        if (!committed_ && !rolled_back_) {
            try {
                rollback();
            } catch (...) {}
        }
    }

    result_void commit() {
        if (rolled_back_) {
            return error_info{-1, "Already rolled back", "transaction"};
        }
        auto result = db_->execute_query("COMMIT");
        if (result) {
            committed_ = true;
        }
        return result;
    }

    result_void rollback() {
        if (committed_) {
            return error_info{-1, "Already committed", "transaction"};
        }
        auto result = db_->execute_query("ROLLBACK");
        if (result) {
            rolled_back_ = true;
        }
        return result;
    }

private:
    database_base* db_;
    bool committed_ = false;
    bool rolled_back_ = false;
};

// Usage:
{
    auto conn = pool->acquire();
    transaction tx(conn.get());

    conn->insert_query("INSERT INTO users...");
    conn->insert_query("INSERT INTO logs...");

    tx.commit();  // Both or neither
}
```

**Priority:** P2
**Effort:** 3-4 days

---

## Testing Requirements

```cpp
TEST(ConnectionPool, NoLeakOnException) {
    auto pool = create_test_pool();

    {
        auto conn = pool->acquire().value();
        // Simulate exception
        throw std::runtime_error("test");
    }  // Connection auto-released

    // Pool should have all connections back
    EXPECT_EQ(pool->available_connections(), pool->total_connections());
}
```

**Total Effort:** 11-16 days
