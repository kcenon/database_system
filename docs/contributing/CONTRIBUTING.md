# Contributing to Database System

**Version:** 0.1.0.0
**Last Updated:** 2025-11-11

Thank you for your interest in contributing to Database System! This guide will help you get started.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Code Style](#code-style)
- [Testing](#testing)
- [Documentation](#documentation)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)

---

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to:

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards other community members

---

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- C++17 compatible compiler (C++20 for async features)
  - **macOS**: Xcode 10+ or Clang 5+
  - **Linux**: GCC 7+ or Clang 5+
  - **Windows**: Visual Studio 2017+ or MinGW-w64
- CMake 3.16 or higher
- Git
- **Database-specific prerequisites**:
  - PostgreSQL development libraries (libpqxx, libpq, OpenSSL)
  - MySQL development libraries (libmysql or mysql-connector-cpp)
  - SQLite development library (sqlite3)
  - MongoDB C++ driver (mongocxx, bsoncxx)
  - Redis client library (hiredis)
- **Optional dependencies**:
  - common_system (for Result<T> and ecosystem integration)
  - thread_system (for high-performance async operations)
  - logger_system (for integrated logging)
  - monitoring_system (for performance metrics)

### First-Time Contributors

New to open source? Start here:

1. **Browse issues labeled `good first issue`**:
   - [Good First Issues](https://github.com/kcenon/database_system/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)

2. **Read the documentation**:
   - [Architecture Guide](../ARCHITECTURE.md)
   - [API Reference](../API_REFERENCE.md)
   - [Build Guide](../BUILD_GUIDE.md)
   - [Improvement Plan](../../IMPROVEMENT_PLAN.md)

3. **Join discussions**:
   - [GitHub Discussions](https://github.com/kcenon/database_system/discussions)

---

## Development Setup

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/database_system.git
cd database_system

# Add upstream remote
git remote add upstream https://github.com/kcenon/database_system.git
```

### 2. Install Dependencies

#### Using vcpkg (Recommended)

```bash
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# OR
bootstrap-vcpkg.bat   # Windows

# Install all database dependencies
vcpkg install libpqxx openssl libmysql sqlite3 mongo-cxx-driver hiredis
```

#### Manual Installation

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install \
    libpqxx-dev libpq-dev libssl-dev \
    libmysqlclient-dev \
    libsqlite3-dev \
    libmongocxx-dev libbsoncxx-dev \
    libhiredis-dev
```

**macOS:**
```bash
brew install libpqxx postgresql openssl mysql sqlite mongo-cxx-driver hiredis
```

**Windows:**
Use vcpkg (recommended) as manual installation is complex.

### 3. Build the Project

```bash
# Create build directory
mkdir build && cd build

# Configure with desired backends
cmake .. \
    -DUSE_POSTGRESQL=ON \
    -DUSE_MYSQL=ON \
    -DUSE_SQLITE=ON \
    -DUSE_MONGODB=ON \
    -DUSE_REDIS=ON \
    -DBUILD_DATABASE_SAMPLES=ON \
    -DUSE_UNIT_TEST=ON

# Build
cmake --build . -j$(nproc)
```

### 4. Run Tests

```bash
# Run all unit tests
ctest --output-on-failure

# Run specific test
ctest -R database_test

# Run with verbose output
ctest -VV
```

### 5. Install Development Tools

**Recommended tools**:

```bash
# Code formatter (clang-format)
# Usually comes with clang installation

# Static analyzers
sudo apt-get install clang-tidy cppcheck  # Linux
brew install clang-tidy cppcheck          # macOS

# Markdown linter
npm install -g markdownlint-cli

# Link checker
npm install -g markdown-link-check
```

---

## Making Changes

### Workflow

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/your-bug-fix
   ```

2. **Make your changes**:
   - Write clean, documented code
   - Follow [code style guidelines](#code-style)
   - Add tests for new features
   - Update documentation

3. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat(backend): add MongoDB aggregation pipeline support"
   ```

4. **Keep your branch updated**:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

### Commit Message Guidelines

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, no logic change)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `build`: Build system changes
- `ci`: CI/CD changes
- `chore`: Maintenance tasks

**Scopes** (database_system specific):
- `core`: Core database abstraction layer
- `backend`: Database backend implementations (postgres, mysql, sqlite, mongodb, redis)
- `orm`: ORM framework
- `pool`: Connection pooling
- `query`: Query builders
- `security`: Security features (TLS, RBAC, audit)
- `monitoring`: Performance monitoring
- `async`: Async operations
- `adapter`: Adapter pattern implementations

**Examples**:

```
feat(backend): add PostgreSQL JSONB support

Implement native JSONB operations for PostgreSQL backend.
Includes query builder integration and ORM mapping.

Closes #123
```

```
fix(pool): resolve connection leak in health checker

Add proper connection release in health check failure path.
Fixes memory leak detected by AddressSanitizer.

Fixes #456
```

```
docs(api): update connection pool configuration examples

- Add new pool sizing recommendations
- Update timeout configuration
- Add multi-backend pool examples
```

```
perf(query): optimize prepared statement caching

Implement LRU cache for prepared statements.
Reduces query latency by 15% in benchmark tests.

Benchmark results:
- Before: 1.2ms avg query time
- After: 1.02ms avg query time
```

---

## Code Style

### C++ Style Guidelines

Database System follows modern C++ best practices:

#### General Rules

1. **C++ Standard**: Use C++17 features (C++20 for optional async/coroutines)
2. **Naming Conventions**:
   - `snake_case` for variables, functions, files
   - `PascalCase` for class names
   - `UPPER_CASE` for constants and macros
   - `_` suffix for private members (optional but preferred)

3. **Formatting**:
   - Indentation: Tabs (not spaces)
   - Line length: 100-120 characters
   - Braces: K&R style (opening brace on same line)

#### Example

```cpp
namespace database {

/**
 * @brief Manages database connections and provides query execution interface
 */
class database_manager {
public:
    database_manager();
    explicit database_manager(std::shared_ptr<database_context> context);
    ~database_manager();

    /**
     * @brief Set the database backend type
     * @param database_type Type of database (postgres, mysql, sqlite, etc.)
     * @return true if successful, false otherwise
     */
    bool set_mode(const database_types& database_type);

    /**
     * @brief Check if database is connected
     * @return true if connected, false otherwise
     */
    bool is_connected() const noexcept;

private:
    void cleanup_connections();

    bool connected_;
    std::unique_ptr<database_base> database_;
    std::shared_ptr<database_context> context_;
    std::shared_ptr<connection_pool_manager> pool_manager_;
};

} // namespace database
```

### Code Quality

1. **No warnings**: Code must compile without warnings (`-Wall -Wextra -Wpedantic`)
2. **Modern C++**: Use RAII, smart pointers, STL algorithms
3. **Const-correctness**: Mark methods and parameters `const` when possible
4. **Exception safety**: Provide at least basic exception safety guarantee
5. **Thread safety**: Document thread-safety guarantees
6. **Resource management**: Use smart pointers (`std::shared_ptr`, `std::unique_ptr`)

### Header Guards

Use `#pragma once` (preferred in this codebase):

```cpp
#pragma once

#include <memory>
#include "database_types.h"

namespace database {
// ... declarations
}
```

### Include Order

```cpp
// 1. Corresponding header (for .cpp files)
#include "database/database_manager.h"

// 2. C system headers
#include <cstdint>
#include <cstring>

// 3. C++ standard library headers
#include <memory>
#include <string>
#include <vector>

// 4. Other library headers
#include <pqxx/pqxx>

// 5. Project headers
#include "database/connection_pool.h"
#include "database/query_builder.h"
```

### Static Analysis

Run static analysis before submitting:

```bash
# Clang-Tidy
clang-tidy src/**/*.cpp -- -std=c++17

# Cppcheck
cppcheck --enable=all --std=c++17 database/
```

### Backend-Specific Guidelines

#### PostgreSQL Backend
- Use prepared statements for parameterized queries
- Leverage JSONB for semi-structured data
- Use CTEs for complex queries

#### MySQL Backend
- Use proper charset/collation (utf8mb4)
- Implement connection pooling
- Handle AUTO_INCREMENT properly

#### SQLite Backend
- Enable WAL mode for concurrent access
- Use FTS5 for full-text search
- Proper transaction handling

#### MongoDB Backend
- Use aggregation pipeline for complex queries
- Implement proper index strategies
- Handle BSON types correctly

#### Redis Backend
- Use pipelining for batch operations
- Implement proper key expiration
- Handle data type conversions

---

## Testing

### Writing Tests

All new features and bug fixes MUST include tests.

#### Test Structure

```cpp
#include <gtest/gtest.h>
#include <database/database_manager.h>

namespace database::test {

TEST(DatabaseManager, BasicConnection) {
    // Arrange
    database_manager manager;

    // Act
    bool result = manager.set_mode(database_types::postgres);

    // Assert
    EXPECT_TRUE(result);
}

TEST(DatabaseManager, ConnectionPooling) {
    // Test connection pool functionality
    database_manager manager;
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 20;

    bool created = manager.create_connection_pool(database_types::postgres, config);
    EXPECT_TRUE(created);

    auto pool = manager.get_connection_pool(database_types::postgres);
    EXPECT_NE(pool, nullptr);
}

TEST(DatabaseManager, ErrorHandling) {
    // Test error cases
    database_manager manager;

    // Should fail - not connected
    auto result = manager.select_query("SELECT * FROM users");
    EXPECT_TRUE(result.empty());
}

TEST(DatabaseManager, ThreadSafety) {
    // Test concurrent access
    database_manager manager;
    manager.set_mode(database_types::sqlite);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager]() {
            auto result = manager.select_query("SELECT 1");
            EXPECT_FALSE(result.empty());
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

} // namespace database::test
```

### Running Tests

```bash
# All tests
ctest --output-on-failure

# Specific test suite
ctest -R ConnectionPoolTest

# With verbose output
ctest -VV

# With sanitizers (recommended before PR)
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" ..
cmake --build .
ctest
```

### Backend-Specific Testing

Each database backend requires specific testing:

#### PostgreSQL Tests
```bash
# Ensure PostgreSQL is running
sudo systemctl start postgresql
# OR
docker run -d -p 5432:5432 -e POSTGRES_PASSWORD=test postgres

# Run PostgreSQL-specific tests
ctest -R postgres_test
```

#### MySQL Tests
```bash
# Ensure MySQL is running
sudo systemctl start mysql
# OR
docker run -d -p 3306:3306 -e MYSQL_ROOT_PASSWORD=test mysql

# Run MySQL-specific tests
ctest -R mysql_test
```

#### SQLite Tests
```bash
# No server required
ctest -R sqlite_test
```

#### MongoDB Tests
```bash
# Ensure MongoDB is running
sudo systemctl start mongod
# OR
docker run -d -p 27017:27017 mongo

# Run MongoDB-specific tests
ctest -R mongodb_test
```

#### Redis Tests
```bash
# Ensure Redis is running
sudo systemctl start redis
# OR
docker run -d -p 6379:6379 redis

# Run Redis-specific tests
ctest -R redis_test
```

### Test Coverage

Aim for >80% code coverage for new code:

```bash
# Generate coverage report
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
cmake --build .
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# Open coverage report
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### Performance Tests

For performance-sensitive changes, provide benchmarks:

```bash
cd benchmarks
cmake ..
cmake --build .
./connection_pool_bench
./query_execution_bench
./transaction_bench
```

Include before/after performance numbers in pull request description.

**Current benchmarks (baseline)**:
- Connection Pool: 0.1ms acquisition time
- Simple SELECT: 1.2ms (PostgreSQL), 0.8ms (SQLite), 0.3ms (Redis)
- Complex JOIN: 15ms (PostgreSQL)
- Bulk INSERT (1K): 45ms (PostgreSQL)

---

## Documentation

### Code Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Execute a SELECT query and return results
 *
 * Executes the given SQL SELECT query and returns the results as a vector
 * of rows. Each row is a map of column names to values. Supports all standard
 * SQL data types with proper type conversion.
 *
 * @param query SQL SELECT query to execute
 * @return Vector of rows (each row is a map of column->value)
 * @throws std::runtime_error If query execution fails or database not connected
 *
 * @example
 * ```cpp
 * auto results = db.select_query("SELECT id, name FROM users WHERE age > 18");
 * for (const auto& row : results) {
 *     auto id = std::get<int64_t>(row.at("id"));
 *     auto name = std::get<std::string>(row.at("name"));
 * }
 * ```
 */
database_result select_query(const std::string& query);
```

### Markdown Documentation

Update documentation when:
- Adding new features
- Changing existing APIs
- Fixing bugs that affect usage
- Adding new backend support
- Modifying security features

**Required updates**:
- [ ] Update README.md if user-facing change
- [ ] Update API_REFERENCE.md for API changes
- [ ] Add entry to CHANGELOG.md
- [ ] Update examples if behavior changes
- [ ] Update Korean translations (_KO.md files)
- [ ] Update BUILD_GUIDE.md for new dependencies

**Documentation checklist**:
```bash
# Lint markdown
markdownlint docs/**/*.md

# Check links
markdown-link-check docs/**/*.md

# Spell check
cspell docs/**/*.md
```

---

## Submitting Changes

### Before Submitting

Ensure your changes meet these criteria:

- [ ] Code compiles without warnings
- [ ] All tests pass (22/23 tests passing, 1 skipped is acceptable)
- [ ] New tests added for new features
- [ ] Code follows style guidelines (tabs, K&R braces, snake_case)
- [ ] Documentation updated
- [ ] Commit messages follow conventions
- [ ] No merge conflicts with `main` branch
- [ ] Static analysis passes (clang-tidy, cppcheck)
- [ ] Backend-specific tests pass (if applicable)

### Creating a Pull Request

1. **Push your branch**:
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Open pull request on GitHub**:
   - Title: Brief description (50 chars or less)
   - Description: Detailed explanation including:
     - What changed and why
     - Related issue numbers (#123)
     - Breaking changes (if any)
     - Testing performed
     - Performance impact (if applicable)
     - Backend compatibility notes

3. **Pull request template**:

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update
- [ ] Backend implementation
- [ ] Performance improvement
- [ ] Security enhancement

## Backend Impact
- [ ] PostgreSQL
- [ ] MySQL
- [ ] SQLite
- [ ] MongoDB
- [ ] Redis
- [ ] Core abstraction layer

## Related Issues
Closes #123

## Testing
- [ ] All tests pass (22/23)
- [ ] Added new tests
- [ ] Manual testing performed
- [ ] Backend-specific testing completed
- [ ] Performance benchmarks run (if applicable)

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-reviewed code
- [ ] Commented complex code
- [ ] Updated documentation
- [ ] No new warnings
- [ ] Added tests for changes
- [ ] All tests passing
- [ ] Thread-safety verified
- [ ] Memory leaks checked (AddressSanitizer)
```

### Draft Pull Requests

For work-in-progress changes:
1. Mark PR as "Draft"
2. Use `[WIP]` prefix in title
3. Request early feedback if needed

---

## Review Process

### What to Expect

1. **Automated checks**: CI/CD runs automatically
   - Build verification (Linux, macOS, Windows)
   - Test suite (all 5 backends)
   - Code style checks
   - Static analysis (clang-tidy, cppcheck)
   - Memory leak detection (AddressSanitizer)
   - Thread safety verification (ThreadSanitizer)

2. **Code review**: Maintainers will review your code
   - Usually within 1-3 business days
   - May request changes
   - Be responsive to feedback
   - Focus on correctness, performance, and security

3. **Approval**: Once approved and checks pass
   - PR will be merged by maintainers
   - Your contribution appears in next release!

### Addressing Feedback

```bash
# Make requested changes
git add .
git commit -m "address review feedback: improve error handling"
git push origin feature/your-feature-name
```

No need to create a new PR - just push to same branch.

### After Merge

```bash
# Update your local main branch
git checkout main
git pull upstream main

# Delete feature branch (optional)
git branch -d feature/your-feature-name
git push origin --delete feature/your-feature-name
```

---

## Development Practices

### Branch Strategy

- `main` - Stable code
- `develop` - Integration branch for next release (if used)
- `feature/*` - New features
- `fix/*` - Bug fixes
- `docs/*` - Documentation updates
- `perf/*` - Performance improvements
- `refactor/*` - Code refactoring

### Release Cycle

**Current Status**: Active development

- **Major releases**: Every 6-12 months (breaking changes, new backends)
- **Minor releases**: Monthly (new features, improvements)
- **Patch releases**: As needed for critical bugs

**Current Version**: 1.0.0 (Phase 2 completed, Phase 3 C++17 migration complete)

### Versioning

Follows [Semantic Versioning](https://semver.org/):
- MAJOR.MINOR.PATCH (e.g., 1.0.0)

**Breaking Changes**: Requires major version bump
**New Features**: Minor version bump
**Bug Fixes**: Patch version bump

---

## Contribution Areas

### High-Priority Areas

1. **Backend Implementations**
   - Improve existing backend performance
   - Add new database backend support
   - Enhance backend plugin architecture

2. **ORM Framework**
   - Enhanced entity mapping
   - Advanced relationship support
   - Schema migration system

3. **Security Features**
   - Additional authentication methods
   - Advanced encryption options
   - Improved audit logging
   - Threat detection enhancements

4. **Performance Optimization**
   - Query optimization algorithms
   - Advanced caching strategies
   - Connection pool improvements
   - Bulk operation enhancements

5. **Testing & Quality**
   - Increase test coverage (target: 90%+)
   - Add more integration tests
   - Improve benchmark coverage
   - Thread safety verification

### Backend-Specific Contributions

#### PostgreSQL
- COPY command support
- Advanced JSONB operations
- Partition table support
- Replication support

#### MySQL
- Full-text search enhancements
- JSON column operations
- Partition management
- Replication support

#### SQLite
- Virtual table support
- Extension loading
- Backup/restore utilities
- Write-ahead logging improvements

#### MongoDB
- Aggregation pipeline optimization
- GridFS support
- Sharding support
- Change streams

#### Redis
- Redis Cluster support
- Redis Streams
- Module support
- Geo-spatial operations

---

## Getting Help

### Resources

- **Documentation**: [docs/](../)
- **Examples**: [samples/](../../samples/)
- **Build Guide**: [docs/BUILD_GUIDE.md](../BUILD_GUIDE.md)
- **Architecture**: [docs/ARCHITECTURE.md](../ARCHITECTURE.md)
- **API Reference**: [docs/API_REFERENCE.md](../API_REFERENCE.md)
- **Improvement Plan**: [IMPROVEMENT_PLAN.md](../../IMPROVEMENT_PLAN.md)

### Communication

- **Questions**: [GitHub Discussions](https://github.com/kcenon/database_system/discussions)
- **Bug reports**: [GitHub Issues](https://github.com/kcenon/database_system/issues)
- **Feature requests**: [GitHub Issues](https://github.com/kcenon/database_system/issues)
- **Security issues**: Email maintainers directly (see SECURITY.md)

---

## Recognition

Contributors are recognized in:
- [CHANGELOG.md](../../CHANGELOG.md) - Listed in release notes
- [GitHub Contributors](https://github.com/kcenon/database_system/graphs/contributors) - Automatically tracked
- Project README - Significant contributors highlighted

---

## License

By contributing to database_system, you agree that your contributions will be licensed under the BSD 3-Clause License.

---

## Thank You!

Your contributions make database_system better for everyone. We appreciate your time and effort!

---

**Maintainers**: [@kcenon](https://github.com/kcenon)
**Last Updated**: 2025-11-11
