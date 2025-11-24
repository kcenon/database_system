# Distributed System Multi-Node Tests

This directory contains comprehensive multi-node tests for the database system's distributed features, including cluster management, failover, replication, and network partition handling.

## Overview

The distributed tests validate:
- **Cluster Formation**: Node discovery, registration, and dynamic scaling
- **Failover**: Automatic and manual failover scenarios
- **Network Partitions**: Split-brain prevention and partition recovery
- **Performance**: Throughput, latency, and replication lag benchmarks

## Test Infrastructure

### Components

1. **Multi-Node Test Framework** (`multi_node_test_framework.h/cpp`)
   - Base class for all distributed tests
   - Docker container management
   - Toxiproxy integration for fault injection
   - Data consistency verification utilities

2. **Test Suites**
   - `cluster_formation_test.cpp`: Cluster setup and node management
   - `failover_test.cpp`: Primary failure and promotion scenarios
   - `network_partition_test.cpp`: Network fault tolerance
   - `distributed_benchmark_test.cpp`: Performance measurements

### Docker Environment

The tests use Docker Compose to create a realistic multi-node environment:
- 1 PostgreSQL primary node (port 5432)
- 2 PostgreSQL replica nodes (ports 5433, 5434)
- 1 MySQL node (port 3306)
- Toxiproxy for network fault injection (port 8474)

## Running the Tests

### Prerequisites

```bash
# Install dependencies
sudo apt-get install -y \
    docker docker-compose \
    cmake ninja-build \
    libpq-dev libmysqlclient-dev libsqlite3-dev \
    libcurl4-openssl-dev \
    libgtest-dev libgmock-dev
```

### Quick Start

```bash
# 1. Start the test cluster
docker-compose -f docker-compose.distributed-test.yml up -d

# 2. Wait for services to be ready
./scripts/wait-for-services.sh

# 3. Build the project
cmake -B build -DENABLE_DISTRIBUTED_TESTS=ON
cmake --build build

# 4. Run distributed tests
cd build
ctest -R "distributed" -V

# 5. Cleanup
docker-compose -f docker-compose.distributed-test.yml down -v
```

### Running Specific Test Suites

```bash
# Cluster formation tests only
ctest -R "ClusterFormation" -V

# Failover tests only
ctest -R "Failover" -V

# Network partition tests only
ctest -R "NetworkPartition" -V

# Performance benchmarks only
ctest -R "DistributedBenchmark" -V
```

### Running Individual Tests

```bash
# List available tests
./bin/cluster_formation_test --gtest_list_tests

# Run specific test
./bin/cluster_formation_test --gtest_filter="ClusterFormationTest.BasicClusterStartup"

# Run with verbose output
./bin/failover_test --gtest_filter="FailoverTest.*" --gtest_color=yes
```

## Test Configuration

### Multi-Node Config

Tests can be configured via the `multi_node_config` struct:

```cpp
multi_node_config config;
config.num_primary_nodes = 1;
config.num_replica_nodes = 2;
config.enable_gateway = true;
config.enable_replication = true;
config.use_toxiproxy = true;  // Enable fault injection
config.startup_timeout = std::chrono::seconds(60);
```

### Environment Variables

- `ENABLE_TOXIPROXY=true`: Enable network fault injection
- `TEST_TIMEOUT=600`: Set test timeout in seconds
- `POSTGRES_HOST=localhost`: Override PostgreSQL host
- `MYSQL_HOST=localhost`: Override MySQL host

## Network Fault Injection

Toxiproxy allows simulating various network conditions:

### Simulating Latency

```cpp
test_base->add_latency("replica1", std::chrono::milliseconds(500));
// ... run tests ...
test_base->remove_latency("replica1");
```

### Simulating Packet Loss

```cpp
test_base->add_packet_loss("replica1", 0.1f);  // 10% loss
// ... run tests ...
test_base->remove_packet_loss("replica1");
```

### Simulating Network Partition

```cpp
test_base->partition_node("replica1");
// ... run tests ...
test_base->heal_partition("replica1");
```

## Performance Benchmarks

### Metrics Collected

- **Throughput**: Operations per second (single/multi-threaded)
- **Latency Distribution**: P50, P95, P99 latencies
- **Replication Lag**: Min/max/avg/median lag times
- **Load Balancing**: Query distribution across replicas

### Interpreting Results

Benchmark output includes:

```
Multi-threaded throughput (10 threads):
  Successful: 950
  Failed: 50
  Duration: 2500 ms
  Throughput: 380.0 ops/sec
  Success rate: 95.0%
```

Expected minimums:
- Single-threaded: > 10 ops/sec
- Multi-threaded: > 50 ops/sec
- Replication lag: < 100ms average
- Success rate: > 80%

## Continuous Integration

Tests run automatically on:
- **Pull requests**: Basic distributed tests
- **Nightly builds**: Full test suite including chaos engineering
- **On-demand**: Via workflow_dispatch

See `.github/workflows/distributed-tests.yml` for details.

## Troubleshooting

### Services Won't Start

```bash
# Check Docker status
docker ps -a

# View logs
docker-compose -f docker-compose.distributed-test.yml logs

# Clean up and retry
docker-compose -f docker-compose.distributed-test.yml down -v
docker system prune -f
```

### Tests Timeout

- Increase `TEST_TIMEOUT` environment variable
- Check if services are actually healthy: `docker ps`
- Verify network connectivity: `docker exec db-postgres-primary pg_isready`

### Flaky Tests

Network-dependent tests may be timing-sensitive:
- Tests have generous timeouts by default
- Use `GTEST_REPEAT=10` to check for flakiness
- Review logs for specific failure patterns

### Port Conflicts

If ports 5432-5434, 3306, or 8474 are in use:

```bash
# Find process using port
sudo lsof -i :5432

# Stop conflicting services
sudo systemctl stop postgresql
```

## Test Coverage

Current coverage areas:

✅ Cluster formation and node management
✅ Manual and automatic failover
✅ Network partition scenarios
✅ Performance benchmarks
✅ Replication lag measurement
✅ Load balancing verification

Future enhancements:

- [ ] Multi-region scenarios
- [ ] Byzantine fault tolerance
- [ ] Backup and restore testing
- [ ] Rolling upgrade scenarios

## Contributing

When adding new distributed tests:

1. Inherit from `MultiNodeTestBase`
2. Configure `multi_node_config` in `SetUp()`
3. Use helper methods for node management
4. Document expected behavior and timing constraints
5. Add CI integration if needed

## References

- [DB-011 Ticket](../../docs/kanban/DB-011-multi-node.md)
- [Database Gateway](../../database/gateway/database_gateway.h)
- [Cluster Manager](../../database/distributed/cluster_manager.h)
- [Replication Manager](../../database/replication/replication_manager.h)
- [Toxiproxy Documentation](https://github.com/shopify/toxiproxy)

## License

BSD 3-Clause License - see LICENSE file for details.
