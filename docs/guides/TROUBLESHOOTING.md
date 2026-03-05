# Database System Troubleshooting Guide

**Version**: 0.1.0.0
**Last Updated**: 2025-11-11
**Status**: Stable
**Audience**: All

Comprehensive troubleshooting guide for the database_system library covering build errors, connection issues, runtime problems, performance bottlenecks, and platform-specific challenges.

## Table of Contents

- [Common Build Errors](#common-build-errors)
- [Connection Issues](#connection-issues)
- [Runtime Errors](#runtime-errors)
- [Performance Problems](#performance-problems)
- [Backend-Specific Issues](#backend-specific-issues)
- [Platform-Specific Issues](#platform-specific-issues)
- [Debug Techniques](#debug-techniques)
- [Getting Help](#getting-help)

---

## Common Build Errors

### C++ Standard Requirement

**Error**:
```
error: 'std::variant' is not available before C++17
error: C++ standard required is not set
```

**Cause**: Compiler not using C++17 or higher. database_system requires C++17 minimum.

**Solution**:
```bash
# Set C++ standard in CMake
cmake .. -DCMAKE_CXX_STANDARD=17

# Or set compiler flags directly
export CXXFLAGS="-std=c++17"
cmake ..

# Verify compiler version
g++ --version    # Should be 7.0+
clang++ --version # Should be 5.0+
```

**Prevention**: Update your build environment regularly.

---

### Missing CMake Version

**Error**:
```
CMake 3.16 or higher is required. You are running version 3.10.2
```

**Cause**: Outdated CMake installation.

**Solution**:
```bash
# Ubuntu/Debian
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | \
  gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt-get update && sudo apt-get install cmake

# macOS
brew upgrade cmake

# Manual build
wget https://github.com/Kitware/CMake/releases/download/v3.26.0/cmake-3.26.0.tar.gz
tar -xzf cmake-3.26.0.tar.gz && cd cmake-3.26.0
./bootstrap && make -j$(nproc) && sudo make install
```

---

### Missing Database Libraries

**Error**:
```
Could NOT find libpqxx (missing: libpqxx_LIBRARY libpqxx_INCLUDE_DIR)
```

**Cause**: Database client library not installed or not in CMake search path.

**Solution**:
```bash
# Option 1: Disable the backend (if not needed)
cmake .. -DUSE_POSTGRESQL=OFF

# Option 2: Install the library
# Ubuntu/Debian
sudo apt-get install libpqxx-dev libpq-dev libssl-dev

# CentOS/RHEL
sudo dnf install libpqxx-devel postgresql-devel openssl-devel

# macOS
brew install libpqxx postgresql

# Option 3: Use vcpkg
vcpkg install libpqxx openssl
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

**Backend Library Matrix**:
| Backend | Package | Ubuntu | macOS |
|---------|---------|--------|-------|
| PostgreSQL | libpqxx | libpqxx-dev | libpqxx |
| MySQL | libmariadb | libmariadb-dev | mariadb-connector-c |
| SQLite | sqlite3 | libsqlite3-dev | sqlite |
| MongoDB | mongocxx | libmongocxx-dev | mongo-cxx-driver |
| Redis | hiredis | libhiredis-dev | hiredis |

---

### Linking Errors

**Error**:
```
undefined reference to 'pqxx::connection::connection(...)'
/usr/bin/ld: cannot find -lpqxx
```

**Cause**: Libraries found but not properly linked or library path not set.

**Solution**:
```bash
# Verify library exists
ldconfig -p | grep libpqxx  # Linux
brew list libpqxx           # macOS

# Add library path to CMake
cmake .. \
  -DCMAKE_PREFIX_PATH="/usr/local/lib:$CMAKE_PREFIX_PATH" \
  -DCMAKE_VERBOSE_MAKEFILE=ON

# Check what was linked
ldd ./bin/basic_usage | grep libpqxx    # Linux
otool -L ./bin/basic_usage | grep libpqxx # macOS

# For static builds
cmake .. -DBUILD_SHARED_LIBS=OFF
```

---

### MongoDB Driver Compilation Failure

**Error**:
```
error: undefined reference to 'mongocxx::v_noabi::client::list_databases()'
```

**Cause**: MongoDB C++ driver version mismatch or incomplete installation.

**Solution**:
```bash
# Reinstall MongoDB C++ driver
git clone https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DBSONCXX_INCLUDE_DIR=/usr/local/include/bsoncxx/v_noabi
make -j$(nproc) && sudo make install

# Update environment
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# Rebuild database_system
cd /path/to/database_system/build
cmake .. && ninja
```

---

## Connection Issues

### Connection Timeout

**Error**:
```cpp
database_manager& db = database_manager::handle();
bool success = db.connect("host=localhost port=5432 dbname=testdb");
// Returns false with connection timeout
```

**Cause**: Database server unreachable, firewall blocked, or server not running.

**Troubleshooting**:
```bash
# Check if server is running
psql -U postgres -h localhost -p 5432 -c "SELECT 1"  # PostgreSQL
mysql -u root -h localhost                            # MySQL
sqlite3 /path/to/database.db "SELECT 1;"            # SQLite

# Check network connectivity
ping localhost
telnet localhost 5432  # Test TCP connection

# Check firewall
sudo netstat -tulpn | grep LISTEN | grep 5432

# View system logs
journalctl -u postgresql -n 50  # PostgreSQL service logs
tail -f /var/log/mysql/error.log # MySQL error log
```

**Solution**:
```cpp
// Increase timeout in connection config
connection_pool_config config;
config.acquire_timeout = std::chrono::milliseconds(10000); // 10 seconds
config.connection_string = "host=localhost port=5432 dbname=testdb";

// Add retry logic
int max_retries = 3;
for (int i = 0; i < max_retries; ++i) {
    if (db.connect(config.connection_string)) {
        break;  // Connection successful
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
```

---

### Authentication Failure

**Error**:
```
FATAL: password authentication failed for user "postgres"
Access denied for user 'root'@'localhost' (using password: YES)
```

**Cause**: Incorrect credentials or authentication method.

**Solution**:

1. **PostgreSQL**:
```bash
# Verify credentials
psql -U postgres -h localhost -W  # Test with password prompt

# Check pg_hba.conf (authentication method)
cat /etc/postgresql/*/main/pg_hba.conf | grep "local\|host"

# Reset password (on Ubuntu with local socket)
sudo -u postgres psql
postgres=# ALTER USER postgres WITH PASSWORD 'new_password';
```

2. **MySQL**:
```bash
# Test connection
mysql -u root -p -h localhost

# Reset root password
sudo mysql_secure_installation

# Create new user
mysql -u root -p
mysql> CREATE USER 'dbuser'@'localhost' IDENTIFIED BY 'password';
mysql> GRANT ALL PRIVILEGES ON testdb.* TO 'dbuser'@'localhost';
mysql> FLUSH PRIVILEGES;
```

3. **Code Configuration**:
```cpp
// Properly format connection string with credentials
std::string conn_str = "host=localhost dbname=testdb user=postgres password=secret";
db.connect(conn_str);
```

---

### SSL/TLS Certificate Issues

**Error**:
```
SSL error: certificate verify failed
SSL: CERTIFICATE_VERIFY_FAILED
```

**Cause**: SSL certificate missing, expired, or self-signed.

**Solution**:
```bash
# For PostgreSQL with self-signed certificates
export PGSSLMODE=require
export PGSSLCERT=/path/to/client-cert.pem
export PGSSLKEY=/path/to/client-key.pem
export PGSSLROOTCERT=/path/to/ca-cert.pem

# In connection string
std::string conn_str = "host=localhost sslmode=require sslcert=/path/to/cert.pem";

# For development (disable verification - NOT for production)
export PGSSLMODE=allow
```

---

## Runtime Errors

### Null Pointer Dereference in Query Results

**Error**:
```cpp
auto result = db.select_query("SELECT * FROM users");
// Segmentation fault when accessing result
```

**Cause**: Query result not validated before access. Result might be empty or null.

**Solution**:
```cpp
// Always validate before accessing
auto result = db.select_query("SELECT * FROM users WHERE id = 1");

// Check using Result<T> pattern
auto query_result = db.select_query("SELECT * FROM users");
if (query_result) {
    // Safe to use result
    for (const auto& row : query_result) {
        // Process row
    }
} else {
    // Handle error
    std::cerr << "Query failed" << std::endl;
}

// Alternative: explicit null check
if (!result.empty()) {
    auto first_row = result.at(0);
    // Use first_row safely
}
```

---

### Connection Pool Exhaustion

**Error**:
```
[ERROR] Connection pool exhausted: no available connections
Failed to acquire connection: timeout exceeded
```

**Cause**: All connections in pool are in use, or connections not properly released.

**Symptoms**:
- Application performance degrades
- New connections timeout
- Memory usage grows

**Diagnosis**:
```cpp
// Monitor pool status
auto stats = db.get_pool_stats();
for (const auto& [db_type, stat] : stats) {
    std::cout << "Active: " << stat.active_connections << std::endl;
    std::cout << "Available: " << stat.available_connections << std::endl;
    std::cout << "Failed acquisitions: " << stat.failed_acquisitions << std::endl;
}
```

**Solution**:
```cpp
// Increase pool size
connection_pool_config config;
config.min_connections = 5;
config.max_connections = 50;  // Increase from default 20
config.acquire_timeout = std::chrono::milliseconds(10000);
config.idle_timeout = std::chrono::milliseconds(30000);

db.create_connection_pool(database_types::postgres, config);

// Ensure connections are released (use RAII pattern)
{
    auto connection = pool->acquire();
    if (connection) {
        // Use connection
        // Automatically released when connection goes out of scope
    }
}  // Connection returned to pool

// Find connection leaks with leak detector
database::connection_leak_detector& detector =
    database::connection_leak_detector::instance();
auto leaks = detector.get_leak_report();
if (!leaks.empty()) {
    std::cerr << "Found " << leaks.size() << " connection leaks" << std::endl;
}
```

---

### Memory Leaks

**Error**:
```
valgrind: LEAK SUMMARY: 1,234 bytes in 10 blocks are definitely lost
```

**Cause**: Improperly managed memory, unclosed connections, or unreleased resources.

**Detection**:
```bash
# Run with valgrind
valgrind --leak-check=full --show-leak-kinds=all ./bin/basic_usage

# Enable address sanitizer (compile-time)
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
```

**Solution**:
```cpp
// Use RAII for resource management
class DatabaseSession {
private:
    std::shared_ptr<database_connection> conn_;
public:
    DatabaseSession() : conn_(pool->acquire()) { }
    ~DatabaseSession() {
        // Automatically released
    }

    database_connection* operator->() { return conn_.get(); }
};

// Proper cleanup
{
    database_manager& db = database_manager::handle();
    db.connect("...");
    // ... use db ...
    db.disconnect();  // Always disconnect
}
```

---

### Race Conditions in Multi-threaded Code

**Error**:
```
Undefined behavior with concurrent access to database_manager
Data corruption in query results
```

**Cause**: database_context and managers are not thread-safe by default.

**Solution**:
```cpp
// Use thread-safe dependency injection
auto context = std::make_shared<database_context>();
auto db_mgr = std::make_shared<database_manager>(context);

// Or use thread-local storage
thread_local database_manager db_instance;

// Synchronize access
std::mutex db_mutex;
{
    std::lock_guard<std::mutex> lock(db_mutex);
    auto result = db.select_query("SELECT ...");
}

// Better: Use connection per thread
void worker_thread() {
    auto context = std::make_shared<database_context>();
    auto db = std::make_shared<database_manager>(context);
    // Each thread has its own database instance
    db->select_query("...");
}
```

---

## Performance Problems

### Slow Query Execution

**Symptoms**:
- Query execution takes >1 second
- Application becomes unresponsive
- CPU utilization low despite slow performance

**Diagnosis**:
```cpp
// Enable query timing
database::performance_monitor& monitor =
    database::performance_monitor::instance();

auto start = std::chrono::steady_clock::now();
auto result = db.select_query("SELECT * FROM large_table WHERE id > 100");
auto elapsed = std::chrono::steady_clock::now() - start;

std::cout << "Query took: "
    << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
    << "ms" << std::endl;

// Check query metrics
auto query_latency = monitor.get_query_latency();
std::cout << "Average latency: " << query_latency << "ms" << std::endl;
```

**Solutions**:

1. **Add Database Indexes**:
```sql
-- PostgreSQL
CREATE INDEX idx_users_id ON users(id);
CREATE INDEX idx_orders_user_id ON orders(user_id);

-- MySQL
ALTER TABLE users ADD INDEX idx_id (id);

-- SQLite
CREATE INDEX idx_users_id ON users(id);
```

2. **Optimize Query**:
```cpp
// Before: N+1 query problem
auto users = db.select_query("SELECT * FROM users");
for (const auto& user : users) {
    auto orders = db.select_query(
        std::string("SELECT * FROM orders WHERE user_id = ") + user.at("id"));
}

// After: Single query with JOIN
auto result = db.select_query(
    "SELECT u.*, o.* FROM users u "
    "LEFT JOIN orders o ON u.id = o.user_id");
```

3. **Use Connection Pooling**:
```cpp
// Reduces connection overhead
connection_pool_config config;
config.min_connections = 5;
config.max_connections = 20;
db.create_connection_pool(database_types::postgres, config);
```

4. **Enable Query Caching**:
```cpp
// Cache frequently accessed data
std::map<std::string, database_result> cache;
const auto& result = cache.count(query)
    ? cache[query]
    : (cache[query] = db.select_query(query));
```

---

### High Memory Usage

**Symptoms**:
- Memory grows continuously over time
- RSS increases without bounds
- Application becomes unresponsive

**Causes**:
- Large result sets in memory
- Connection pool not releasing idle connections
- Unclosed database resources

**Solutions**:
```cpp
// 1. Process large results incrementally
void process_large_result() {
    const size_t batch_size = 1000;
    size_t offset = 0;

    while (true) {
        std::string query = "SELECT * FROM huge_table LIMIT " +
                           std::to_string(batch_size) + " OFFSET " +
                           std::to_string(offset);

        auto result = db.select_query(query);
        if (result.empty()) break;

        for (const auto& row : result) {
            process_row(row);
        }

        offset += batch_size;
    }
}

// 2. Monitor memory usage
#include <iostream>
#include <fstream>

size_t get_rss() {
    std::ifstream stat("/proc/self/stat");
    unsigned long vsize, rss;
    for (int i = 0; i < 22; ++i) stat >> vsize;
    stat >> rss;
    return rss * sysconf("_SC_PAGE_SIZE");  // Convert to bytes
}

std::cout << "Memory: " << get_rss() / 1024 / 1024 << " MB" << std::endl;

// 3. Configure connection pool idle timeout
connection_pool_config config;
config.idle_timeout = std::chrono::milliseconds(30000);  // 30 seconds
config.enable_health_checks = true;
```

---

## Backend-Specific Issues

### PostgreSQL Issues

**Connection refused**:
```bash
# Check if PostgreSQL is running
sudo systemctl status postgresql

# Verify listening on port
sudo netstat -tulpn | grep 5432

# Check pg_hba.conf for authentication
sudo -u postgres psql -c "SHOW hba_file"
cat /etc/postgresql/*/main/pg_hba.conf
```

**Cannot create temporary tables**:
```cpp
// Add permissions
std::string grant_sql =
    "GRANT TEMPORARY ON DATABASE testdb TO postgres";
db.create_query(grant_sql);
```

---

### MySQL Issues

**Lost connection during query**:
```cpp
// Implement reconnection logic
bool execute_with_retry(const std::string& query, int max_retries = 3) {
    for (int i = 0; i < max_retries; ++i) {
        try {
            db.insert_query(query);
            return true;
        } catch (const std::exception& e) {
            if (i < max_retries - 1) {
                db.disconnect();
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * i));
                db.connect(connection_string);
            }
        }
    }
    return false;
}
```

**Incorrect datetime conversion**:
```cpp
// Use ISO 8601 format for compatibility
auto query = "INSERT INTO events(event_time) VALUES('2025-11-11T10:30:00')";
db.insert_query(query);

// Better: Use prepared statements
auto formatted_time = std::format("{:%FT%T}", current_time);
```

---

### SQLite Issues

**Database locked**:
```cpp
// Increase busy timeout
sqlite3* db_handle;  // Your SQLite connection
sqlite3_busy_timeout(db_handle, 5000);  // 5 seconds
```

**WAL (Write-Ahead Logging) corruption**:
```bash
# Disable WAL mode
sqlite3 /path/to/database.db "PRAGMA journal_mode=DELETE;"

# Or vacuum and optimize
sqlite3 /path/to/database.db "VACUUM; ANALYZE;"
```

---

### MongoDB Issues

**Cannot connect to replica set**:
```cpp
// Use connection string with multiple hosts
std::string conn_str =
    "mongodb://host1:27017,host2:27017,host3:27017/?replicaSet=rs0";
db.connect(conn_str);
```

**GridFS file too large**:
```cpp
// MongoDB BSON has 16MB document limit
// Use GridFS for files > 16MB
// Split large operations into chunks
```

---

### Redis Issues

**Connection reset by peer**:
```bash
# Check Redis connection
redis-cli PING

# Monitor Redis memory
redis-cli INFO memory

# Increase timeout in code
connection_pool_config config;
config.acquire_timeout = std::chrono::milliseconds(10000);
```

**Out of memory**:
```bash
# Configure Redis eviction policy
redis-cli CONFIG SET maxmemory-policy allkeys-lru

# Monitor usage
redis-cli --memkeys
```

---

## Platform-Specific Issues

### macOS

**Homebrew library not found**:
```bash
# Update Homebrew and packages
brew update
brew upgrade

# Reinstall missing package
brew reinstall libpqxx

# Verify installation
brew list libpqxx
ls -la /usr/local/opt/libpqxx/lib
```

**Code signing issues** (M1/M2 Macs):
```bash
# Build with correct architecture
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64

# Or use universal binary
cmake .. -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

---

### Linux

**Library search path issues**:
```bash
# Add library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
echo "export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH" >> ~/.bashrc

# Update ldconfig cache
sudo ldconfig

# Verify path
ldconfig -p | grep libpqxx
```

**SELinux blocking connections**:
```bash
# Check SELinux status
getenforce

# Temporarily disable (for testing)
sudo setenforce 0

# Permanently: edit /etc/selinux/config
# Set SELINUX=disabled
```

---

### Windows

**DLL dependencies missing**:
```batch
# Use vcpkg for reliable dependency management
vcpkg install libpqxx:x64-windows

# Add to CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake

# Copy DLLs to executable directory
xcopy "C:\path\to\vcpkg\installed\x64-windows\bin\*.dll" ".\build\bin\" /Y
```

**Visual Studio linker errors**:
```cmake
# Use Visual Studio generator
cmake .. -G "Visual Studio 16 2019" -A x64

# Build with correct configuration
cmake --build . --config Release
```

---

## Debug Techniques

### Using GDB (Linux/macOS)

```bash
# Compile with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-g -O0"

# Run with GDB
gdb ./bin/basic_usage

# GDB commands
(gdb) run                      # Start program
(gdb) break database.cpp:123   # Set breakpoint
(gdb) continue                 # Resume execution
(gdb) print variable_name      # Print variable
(gdb) backtrace                # Print stack trace
(gdb) thread apply all bt      # All thread stacks
```

---

### Using Valgrind (Linux)

```bash
# Check for memory leaks
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./bin/basic_usage

# Check for race conditions
valgrind --tool=helgrind ./bin/basic_usage
```

---

### Using AddressSanitizer

```bash
# Enable address sanitizer
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g"

# Run and check output for issues
./bin/basic_usage
```

---

### Custom Debug Output

```cpp
// Add logging to trace issues
#define DB_DEBUG(msg) \
    std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " \
              << msg << std::endl

// Example
DB_DEBUG("Acquiring connection from pool");
auto conn = pool->acquire();
DB_DEBUG("Connection acquired, executing query");
```

---

## Getting Help

### Before Reporting Issues

1. **Verify your setup**:
```bash
# Check C++ version
g++ --version

# Check CMake version
cmake --version

# Test database connectivity
psql --version
mysql --version
sqlite3 --version

# Check library availability
ldconfig -p | grep database_libraries
```

2. **Collect diagnostic information**:
```bash
# System info
uname -a
cmake --version

# Build log
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
make 2>&1 | tee build.log

# Runtime log
./bin/basic_usage 2>&1 | tee runtime.log

# Database version
psql --version
mysql --version
```

3. **Try minimal reproduction**:
```cpp
// Simplest possible code that demonstrates the issue
database_manager& db = database_manager::handle();
if (!db.connect("host=localhost dbname=test")) {
    std::cerr << "Connection failed" << std::endl;
    return 1;
}
auto result = db.select_query("SELECT 1");
```

### Reporting Issues

Include:
1. Database type and version
2. Operating system and version
3. Compiler version
4. CMake configuration used
5. Complete error message or output
6. Minimal reproduction code
7. Attach `build.log` and `runtime.log`

### Additional Resources

- **GitHub Issues**: [database_system issues](https://github.com/kcenon/database_system/issues)
- **Documentation**: See [BUILD_GUIDE.md](../BUILD_GUIDE.md) and [API_REFERENCE.md](../API_REFERENCE.md)
- **FAQ**: Check [FAQ.md](FAQ.md) for common questions
- **Examples**: See `samples/` directory for working examples

---

**Last Updated**: 2025-11-11
