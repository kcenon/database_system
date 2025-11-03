# Phase 2 Checklist: Logger Adapter Implementation

**Duration**: 2-3 days
**Status**: ⏳ Not Started
**Dependencies**: Phase 1 ✅

## 📋 Tasks

### 1. Logger Adapter Header

- [ ] Create `database/integrated/adapters/logger_adapter.h`
- [ ] Add BSD 3-Clause license
- [ ] Add Doxygen documentation
- [ ] Include required headers
  - [ ] `<string>`, `<memory>`, `<chrono>`
  - [ ] `<kcenon/common/patterns/result.h>`
  - [ ] `../core/configuration.h`

#### 1.1 Class Definition

- [ ] Define `logger_adapter` class
- [ ] PIMPL idiom: Forward declare `class impl;`
- [ ] Private member: `std::unique_ptr<impl> pimpl_;`
- [ ] Delete copy constructor/assignment
- [ ] Declare move constructor/assignment as noexcept

#### 1.2 Public Interface

- [ ] Constructor: `explicit logger_adapter(const db_logger_config&)`
- [ ] Destructor: `~logger_adapter()`
- [ ] Lifecycle methods:
  - [ ] `common::VoidResult initialize()`
  - [ ] `common::VoidResult shutdown()`
  - [ ] `bool is_initialized() const`

#### 1.3 Database-Specific Logging Methods

- [ ] `void log_query(db_log_level, const string& query, chrono::microseconds duration)`
- [ ] `void log_slow_query(const string& query, chrono::microseconds duration, chrono::milliseconds threshold)`
- [ ] `void log_connection_event(const string& event, const string& details)`
- [ ] `void log_transaction(const string& operation, bool success, const string& details)`
- [ ] `void log_pool_event(const string& event, size_t active, size_t idle)`
- [ ] `void log_error(const string& operation, const string& error_msg, const string& sql_state)`

#### 1.4 Generic Logging

- [ ] `void log(db_log_level level, const string& message)`
- [ ] `void flush()`

---

### 2. Logger Adapter Implementation

#### 2.1 Create Implementation File

- [ ] Create `database/integrated/adapters/logger_adapter.cpp`
- [ ] Include logger_adapter.h
- [ ] Conditional includes:
  ```cpp
  #if defined(USE_LOGGER_SYSTEM)
      #include <kcenon/logger/core/logger.h>
      #include <kcenon/logger/writers/console_writer.h>
      #include <kcenon/logger/writers/file_writer.h>
  #else
      #include <iostream>
      #include <fstream>
      #include <mutex>
      #include <iomanip>
  #endif
  ```

#### 2.2 PIMPL Implementation (WITH logger_system)

- [ ] Define `logger_adapter::impl` class inside `#if defined(USE_LOGGER_SYSTEM)`
- [ ] Private members:
  - [ ] `const db_logger_config& config_`
  - [ ] `bool initialized_`
  - [ ] `std::unique_ptr<kcenon::logger::logger> logger_`
- [ ] Implement `initialize()`:
  - [ ] Create async logger with buffer size from config
  - [ ] Add console_writer if enabled
  - [ ] Add file_writer if enabled (use config.log_directory)
  - [ ] Set min log level
  - [ ] Call logger_->start()
  - [ ] Set initialized_ = true
- [ ] Implement `shutdown()`:
  - [ ] Call logger_->stop()
  - [ ] Flush remaining logs
  - [ ] Set initialized_ = false
- [ ] Implement logging methods using logger_->write()

#### 2.3 PIMPL Implementation (Fallback)

- [ ] Define `logger_adapter::impl` class inside `#else`
- [ ] Private members:
  - [ ] `const db_logger_config& config_`
  - [ ] `bool initialized_`
  - [ ] `std::mutex mutex_`
  - [ ] `std::ofstream log_file_`
- [ ] Implement `initialize()`:
  - [ ] Open log file if file logging enabled
  - [ ] Set initialized_ = true
- [ ] Implement `shutdown()`:
  - [ ] Close log file
  - [ ] Set initialized_ = false
- [ ] Implement logging methods:
  - [ ] Lock mutex
  - [ ] Format timestamp
  - [ ] Format log level
  - [ ] Write to cout and/or file

#### 2.4 SQL Sanitization

- [ ] Implement `sanitize_query()` private method:
  - [ ] Remove/mask password patterns
  - [ ] Truncate queries longer than 500 chars
  - [ ] Escape special characters for log safety
  - [ ] Return sanitized string

#### 2.5 Slow Query Detection

- [ ] In `log_query()`:
  - [ ] Check if duration > slow_query_threshold
  - [ ] If yes, call `log_slow_query()` automatically
  - [ ] Log at WARNING level

#### 2.6 Log Level Conversion

- [ ] Implement helper function to convert `db_log_level` to logger_system's log_level:
  ```cpp
  #if defined(USE_LOGGER_SYSTEM)
  kcenon::logger::log_level convert_log_level(db_log_level level) {
      switch (level) {
          case db_log_level::trace: return kcenon::logger::log_level::trace;
          // ... other cases
      }
  }
  #endif
  ```

#### 2.7 Public Method Implementations

- [ ] Implement constructor (forwards to impl)
- [ ] Implement destructor (calls shutdown if initialized)
- [ ] Implement move constructor/assignment
- [ ] Implement all public methods (delegate to pimpl_)

---

### 3. Integration into connection_pool_v2

#### 3.1 Update connection_pool_v2.h

- [ ] Add forward declaration or include: `logger_adapter`
- [ ] Add private member: `logger_adapter* logger_`
- [ ] Update constructor to accept `logger_adapter*`

#### 3.2 Update connection_pool_v2.cpp

- [ ] Log connection acquisition:
  ```cpp
  void acquire_connection(connection_priority priority) {
      if (logger_) {
          logger_->log(db_log_level::debug,
              "Acquiring connection with priority " +
              std::to_string(static_cast<int>(priority)));
      }
      // ...
  }
  ```
- [ ] Log connection release
- [ ] Log pool resize events
- [ ] Log health check results
- [ ] Log errors with error codes

#### 3.3 Update Samples

- [ ] Update `samples/migration/connection_pool_v2_demo.cpp`:
  - [ ] Create logger_adapter instance
  - [ ] Pass to connection_pool_v2
  - [ ] Show logs in console

---

### 4. Unit Tests

#### 4.1 Logger Adapter Tests

- [ ] Create `tests/integrated/test_logger_adapter.cpp`
- [ ] Test initialization:
  - [ ] With logger_system available
  - [ ] Without logger_system (fallback)
- [ ] Test logging methods:
  - [ ] `log()` at all levels
  - [ ] `log_query()` with normal query
  - [ ] `log_slow_query()` triggered automatically
  - [ ] `log_connection_event()`
  - [ ] `log_transaction()`
  - [ ] `log_pool_event()`
  - [ ] `log_error()`
- [ ] Test SQL sanitization:
  - [ ] Password patterns removed
  - [ ] Long queries truncated
  - [ ] Special characters escaped
- [ ] Test shutdown:
  - [ ] Logs flushed before shutdown
  - [ ] File closed properly
  - [ ] Can't log after shutdown

#### 4.2 Integration Tests

- [ ] Create `tests/integration/test_logger_integration.cpp`
- [ ] Test logging from connection_pool_v2:
  - [ ] Acquire connection logs event
  - [ ] Release connection logs event
  - [ ] Pool resize logs event
  - [ ] Health check logs result
- [ ] Test with real database (if available):
  - [ ] Execute query and verify log entry
  - [ ] Execute slow query and verify warning
  - [ ] Transaction logs

#### 4.3 Build Configuration Tests

- [ ] Test build with `USE_LOGGER_SYSTEM=ON`:
  - [ ] Verify logger_system headers included
  - [ ] Verify logger_system library linked
  - [ ] Verify no fallback code compiled
- [ ] Test build with `USE_LOGGER_SYSTEM=OFF`:
  - [ ] Verify fallback code compiles
  - [ ] Verify no logger_system dependencies
  - [ ] Verify std::cout logging works

---

### 5. Documentation

- [ ] Update `INTEGRATION.md`:
  - [ ] Add logger_system integration section
  - [ ] Show code examples
  - [ ] Document configuration options
- [ ] Add Doxygen comments to all public methods
- [ ] Create usage example: `examples/integrated/logger_usage.cpp`
- [ ] Document fallback behavior

---

### 6. Code Quality

- [ ] Run clang-tidy: `clang-tidy database/integrated/adapters/logger_adapter.cpp`
- [ ] Run cppcheck: `cppcheck database/integrated/adapters/`
- [ ] Check for compiler warnings (GCC, Clang, MSVC if available)
- [ ] Verify no memory leaks (ASan):
  ```bash
  cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ...
  ./test_logger_adapter
  ```
- [ ] Verify thread safety (TSan):
  ```bash
  cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ...
  ```

---

## ✅ Definition of Done

- [ ] All tasks checked off
- [ ] logger_adapter compiles with USE_LOGGER_SYSTEM=ON
- [ ] logger_adapter compiles with USE_LOGGER_SYSTEM=OFF
- [ ] Fallback logging functional
- [ ] SQL sanitization working
- [ ] Slow query detection working
- [ ] connection_pool_v2 integration complete
- [ ] All unit tests passing
- [ ] All integration tests passing
- [ ] No compiler warnings
- [ ] Static analysis clean
- [ ] No memory leaks or data races
- [ ] Documentation updated
- [ ] Code reviewed

---

**Phase Started**: YYYY-MM-DD
**Phase Completed**: YYYY-MM-DD
**Actual Duration**: N days
