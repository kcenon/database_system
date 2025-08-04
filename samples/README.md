# Database System Samples

This directory contains example programs demonstrating the Database System's capabilities with PostgreSQL integration.

## Available Samples

### 1. Basic Usage (`basic_usage.cpp`)
Demonstrates fundamental database operations:
- Database manager creation and configuration
- Connection management with PostgreSQL
- Table creation with constraints and data types
- CRUD operations (Create, Read, Update, Delete)
- Transaction management with commit/rollback
- Connection health monitoring

**Usage:**
```bash
./basic_usage
```

### 2. PostgreSQL Advanced Features (`postgres_advanced.cpp`)
Shows PostgreSQL-specific advanced features:
- Array operations and queries
- JSONB data manipulation and queries
- Common Table Expressions (CTEs)
- Full-text search with tsvector
- Window functions and analytics
- Advanced indexing strategies
- Complex nested queries

**Usage:**
```bash
./postgres_advanced
```

### 3. Connection Pool Demo (`connection_pool_demo.cpp`)
Connection management and concurrent access examples:
- Single and multiple connection management
- Concurrent database access with multiple threads
- Connection resilience and recovery testing
- Load testing under sustained operations
- Health monitoring and reconnection capabilities
- Performance metrics for concurrent operations

**Usage:**
```bash
./connection_pool_demo
```

### 4. Run All Samples (`run_all_samples.cpp`)
Utility to run all samples or a specific sample:

**Usage:**
```bash
# Run all samples
./run_all_samples

# Run specific sample
./run_all_samples basic_usage
./run_all_samples postgres_advanced
./run_all_samples connection_pool_demo

# List available samples
./run_all_samples --list

# Show help
./run_all_samples --help
```

## Building the Samples

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16 or later
- Database System library
- PostgreSQL client library (libpq)
- PostgreSQL server (for actual database operations)

### Build Instructions

1. **From the main project directory:**
```bash
mkdir build && cd build
cmake .. -DBUILD_DATABASE_SAMPLES=ON
make
```

2. **Run samples:**
```bash
cd bin
./basic_usage
./postgres_advanced
./connection_pool_demo
./run_all_samples
```

### Alternative Build (samples only)
```bash
cd samples
mkdir build && cd build
cmake ..
make
```

## Database Setup

### PostgreSQL Setup for Samples
The samples expect a PostgreSQL database with the following configuration:

```sql
-- Create test database and user
CREATE DATABASE testdb;
CREATE USER testuser WITH PASSWORD 'testpass';
GRANT ALL PRIVILEGES ON DATABASE testdb TO testuser;
```

### Connection Configuration
Update the connection string in the samples if your PostgreSQL setup differs:
```cpp
std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
```

### Running Without Database
The samples are designed to gracefully handle connection failures, showing:
- API usage patterns
- Expected behavior descriptions
- Error handling demonstrations

## Sample Output Examples

### Basic Usage Output
```
=== Database System - Basic Usage Example ===

1. Database Manager Setup:
Database type set to: PostgreSQL
Connection string configured

2. Connection Management:
✓ Successfully connected to database
Connection status: Connected

3. Table Operations:
✓ Users table created successfully

4. Data Insertion:
✓ User inserted successfully
✓ User inserted successfully
...
```

### PostgreSQL Advanced Features Output
```
=== Database System - PostgreSQL Advanced Features Example ===

1. PostgreSQL Manager Setup:
PostgreSQL manager created

2. Advanced Table Creation:
✓ Advanced products table created successfully
✓ Index created

3. PostgreSQL Array Operations:
✓ Product with arrays and JSON inserted
Products with 'gaming' tag:
Gaming Laptop | {gaming,laptop,computer}
...
```

### Connection Pool Demo Output
```
=== Database System - Connection Pool Demo ===

1. Single Connection Demo:
✓ Single connection established
✓ Test table ready

2. Multiple Connections Demo:
Creating 5 database connections...
✓ Connection 1 established
✓ Connection 2 established
...

3. Concurrent Access Demo:
Starting 4 concurrent threads...
Each thread will perform 50 operations

Concurrent access results:
  Successful connections: 4/4
  Total operations attempted: 200
  Successful operations: 195
  Success rate: 97.50%
  Total time: 2340 ms
  Operations per second: 83.33
...
```

## Understanding the Results

### Performance Metrics
- **Operations per second**: Higher is better for throughput
- **Connection success rate**: Should be close to 100%
- **Response time**: Lower is better for latency
- **Concurrent operation success**: Indicates thread safety

### PostgreSQL Features
- **Array Operations**: Demonstrates PostgreSQL's native array support
- **JSONB Queries**: Shows efficient JSON document storage and querying
- **Full-text Search**: PostgreSQL's powerful text search capabilities
- **Window Functions**: Advanced analytics and ranking functions

### Connection Management
- **Connection Pooling**: Efficient resource utilization
- **Health Monitoring**: Proactive connection maintenance
- **Resilience Testing**: Recovery from connection failures
- **Load Testing**: Performance under concurrent access

## Advanced Usage

### Customizing Database Configuration
Modify connection parameters in each sample:
```cpp
// For SSL connections
std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass sslmode=require";

// For different PostgreSQL instance
std::string connection_string = "host=remote-db.example.com port=5432 dbname=proddb user=produser password=securepass";
```

### Performance Tuning
Adjust benchmark parameters in `connection_pool_demo.cpp`:
```cpp
const int num_threads = 8;        // Increase for more concurrency
const int operations_per_thread = 100;  // More operations per thread
const int load_operations = 1000; // Sustained load test size
```

### Adding New Samples
1. Create a new `.cpp` file in the samples directory
2. Add it to the `SAMPLE_PROGRAMS` list in `CMakeLists.txt`
3. Include it in the `run_all_samples.cpp` samples registry

## Troubleshooting

### Common Issues

1. **Connection Failures**
   ```
   ✗ Failed to connect to database
   ```
   - Verify PostgreSQL server is running
   - Check connection parameters (host, port, database, user, password)
   - Ensure database and user exist with proper permissions
   - Check firewall and network connectivity

2. **Permission Errors**
   ```
   ✗ Failed to create users table
   ```
   - Ensure user has CREATE TABLE privileges
   - Check schema permissions
   - Verify database ownership or privileges

3. **Compilation Errors**
   - Ensure C++20 support is enabled
   - Check that database system library is properly linked
   - Verify PostgreSQL client library (libpq) is installed
   - Ensure all required headers are included

4. **Runtime Library Errors**
   - Check that the database system library is built
   - Ensure proper library paths are set (LD_LIBRARY_PATH on Linux, PATH on Windows)
   - Verify PostgreSQL client library is available

### Performance Considerations
- Results may vary based on:
  - Database server performance
  - Network latency (for remote databases)
  - System hardware (CPU, memory, disk I/O)
  - Concurrent database load
  - PostgreSQL configuration and tuning

### Getting Help
- Check the main project README for detailed build instructions
- Review the API documentation for database system usage
- Examine the sample source code for implementation details
- Check PostgreSQL documentation for database-specific features

## PostgreSQL Feature Reference

### Supported PostgreSQL Features
- **Data Types**: SERIAL, VARCHAR, TEXT, INTEGER, DECIMAL, BOOLEAN, TIMESTAMP, ARRAY, JSONB
- **Indexes**: B-tree, GIN (for arrays and JSONB), full-text search indexes
- **Queries**: Complex SELECT with JOINs, CTEs, window functions, subqueries
- **Full-text Search**: tsvector, tsquery, ranking functions
- **JSON Operations**: JSONB storage, operators (->>, ->, @>), indexing
- **Array Operations**: Array construction, containment operators, ANY/ALL
- **Transactions**: BEGIN, COMMIT, ROLLBACK, savepoints
- **Advanced SQL**: Window functions, CTEs, advanced aggregations

### Sample Query Examples
The samples demonstrate various PostgreSQL features:

```sql
-- Array operations
SELECT name FROM products WHERE 'gaming' = ANY(tags);

-- JSONB queries
SELECT name, metadata->>'brand' FROM products WHERE metadata @> '{"warranty": "2 years"}';

-- Window functions
SELECT name, price, ROW_NUMBER() OVER (ORDER BY price DESC) FROM products;

-- Full-text search
SELECT name, ts_rank(search_vector, query) FROM products, plainto_tsquery('laptop') query WHERE search_vector @@ query;

-- Common Table Expressions
WITH expensive_products AS (SELECT * FROM products WHERE price > 100)
SELECT category_id, COUNT(*) FROM expensive_products GROUP BY category_id;
```

## License
These samples are provided under the same BSD 3-Clause License as the Database System project.