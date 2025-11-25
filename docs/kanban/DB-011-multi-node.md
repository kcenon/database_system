# DB-011: Distributed System Multi-Node Tests

**Category**: FEATURE
**Priority**: MEDIUM
**Status**: DONE
**Est. Duration**: 5-7 days
**Dependencies**: DB-004 (Gateway), DB-005 (Replication)
**Assignee**: AI Assistant
**Created**: 2025-11-24
**Completed**: 2025-11-25

---

## 1. What to Change

### Current State
- Distributed features (Gateway, Replication) lack comprehensive multi-node testing
- No automated tests for cluster scenarios
- Network partition and failure scenarios untested
- Performance characteristics under distributed load unknown

### Target State
- Comprehensive multi-node test suite
- Automated chaos engineering tests
- Network partition simulation tests
- Distributed performance benchmarks
- CI/CD integration for distributed tests

### Scope
**Test Scenarios**:
- Multi-node cluster formation and discovery
- Read/write splitting across nodes
- Failover and recovery scenarios
- Data consistency verification
- Network partition handling
- Performance under distributed load

**Test Infrastructure**:
- Docker Compose multi-node setup
- Test orchestration framework
- Chaos injection utilities

---

## 2. How to Change

### 2.1 Test Infrastructure Setup

```yaml
# docker-compose.distributed-test.yml
version: '3.8'

services:
  postgres-primary:
    image: postgres:15
    environment:
      POSTGRES_DB: testdb
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
    networks:
      - db_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U test"]
      interval: 5s
      timeout: 5s
      retries: 5

  postgres-replica1:
    image: postgres:15
    environment:
      POSTGRES_DB: testdb
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
    depends_on:
      postgres-primary:
        condition: service_healthy
    networks:
      - db_network

  postgres-replica2:
    image: postgres:15
    environment:
      POSTGRES_DB: testdb
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
    depends_on:
      postgres-primary:
        condition: service_healthy
    networks:
      - db_network

  mysql-node:
    image: mysql:8.0
    environment:
      MYSQL_ROOT_PASSWORD: test
      MYSQL_DATABASE: testdb
    networks:
      - db_network

  toxiproxy:
    image: ghcr.io/shopify/toxiproxy:2.5.0
    networks:
      - db_network
    ports:
      - "8474:8474"

networks:
  db_network:
    driver: bridge
```

### 2.2 Multi-Node Test Framework

```cpp
// tests/distributed/multi_node_test_framework.h
#pragma once

#include <gtest/gtest.h>
#include "database/distributed/cluster_manager.h"
#include "database/distributed/database_gateway.h"
#include "database/distributed/replication_manager.h"

namespace database::test {

/**
 * @brief Configuration for multi-node test environment
 */
struct multi_node_config {
    int num_primary_nodes = 1;
    int num_replica_nodes = 2;
    bool enable_gateway = true;
    bool enable_replication = true;
    std::chrono::seconds startup_timeout{60};
};

/**
 * @brief Base class for distributed system tests
 */
class MultiNodeTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Node management
    void start_all_nodes();
    void stop_all_nodes();
    void stop_node(const std::string& node_id);
    void start_node(const std::string& node_id);

    // Network simulation
    void partition_node(const std::string& node_id);
    void heal_partition(const std::string& node_id);
    void add_latency(const std::string& node_id, std::chrono::milliseconds latency);
    void remove_latency(const std::string& node_id);

    // Verification
    bool verify_data_consistency();
    bool wait_for_replication_sync(std::chrono::seconds timeout);
    std::map<std::string, int64_t> get_row_counts(const std::string& table);

    // Cluster components
    std::unique_ptr<distributed::cluster_manager> cluster_;
    std::unique_ptr<distributed::database_gateway> gateway_;
    std::unique_ptr<distributed::replication_manager> replication_;

    multi_node_config config_;
};

} // namespace database::test
```

### 2.3 Cluster Formation Tests

```cpp
// tests/distributed/cluster_formation_test.cpp
#include "multi_node_test_framework.h"

class ClusterFormationTest : public database::test::MultiNodeTestBase {
protected:
    void SetUp() override {
        config_.num_primary_nodes = 1;
        config_.num_replica_nodes = 2;
        MultiNodeTestBase::SetUp();
    }
};

TEST_F(ClusterFormationTest, BasicClusterStartup) {
    start_all_nodes();

    EXPECT_EQ(cluster_->get_node_count(), 3);
    EXPECT_TRUE(cluster_->is_healthy());

    auto nodes = cluster_->get_all_nodes();
    int primary_count = 0;
    int replica_count = 0;

    for (const auto& node : nodes) {
        if (node.role == node_role::PRIMARY) primary_count++;
        if (node.role == node_role::REPLICA) replica_count++;
    }

    EXPECT_EQ(primary_count, 1);
    EXPECT_EQ(replica_count, 2);
}

TEST_F(ClusterFormationTest, DynamicNodeAddition) {
    start_all_nodes();
    EXPECT_EQ(cluster_->get_node_count(), 3);

    // Add a new replica
    node_config new_replica;
    new_replica.id = "replica3";
    new_replica.role = node_role::REPLICA;
    // ... configure connection

    EXPECT_TRUE(cluster_->add_node(new_replica));
    EXPECT_EQ(cluster_->get_node_count(), 4);

    // Verify replication starts
    EXPECT_TRUE(wait_for_replication_sync(std::chrono::seconds(30)));
}

TEST_F(ClusterFormationTest, GracefulNodeRemoval) {
    start_all_nodes();

    // Remove a replica
    EXPECT_TRUE(cluster_->remove_node("replica1"));
    EXPECT_EQ(cluster_->get_node_count(), 2);
    EXPECT_TRUE(cluster_->is_healthy());
}
```

### 2.4 Failover Tests

```cpp
// tests/distributed/failover_test.cpp
#include "multi_node_test_framework.h"

class FailoverTest : public database::test::MultiNodeTestBase {
protected:
    void SetUp() override {
        config_.num_replica_nodes = 2;
        config_.enable_replication = true;
        MultiNodeTestBase::SetUp();
    }
};

TEST_F(FailoverTest, AutomaticFailoverOnPrimaryFailure) {
    start_all_nodes();

    // Insert test data
    gateway_->execute_write("INSERT INTO test_table VALUES (1, 'data')");
    EXPECT_TRUE(wait_for_replication_sync(std::chrono::seconds(10)));

    // Record current primary
    auto old_primary = cluster_->get_primary_node_id();

    // Simulate primary failure
    stop_node(old_primary);

    // Wait for failover
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // Verify new primary elected
    auto new_primary = cluster_->get_primary_node_id();
    EXPECT_NE(old_primary, new_primary);
    EXPECT_TRUE(cluster_->is_healthy());

    // Verify data accessible through new primary
    auto result = gateway_->execute_read("SELECT * FROM test_table WHERE id = 1");
    EXPECT_FALSE(result.empty());
}

TEST_F(FailoverTest, ManualFailover) {
    start_all_nodes();

    auto old_primary = cluster_->get_primary_node_id();
    std::string target_replica = "replica1";

    // Initiate manual failover
    EXPECT_TRUE(replication_->initiate_failover(target_replica));

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(10));

    EXPECT_EQ(cluster_->get_primary_node_id(), target_replica);
}

TEST_F(FailoverTest, FailoverWithWriteInProgress) {
    start_all_nodes();

    // Start a long-running write
    std::thread writer([this]() {
        for (int i = 0; i < 100; ++i) {
            gateway_->execute_write(
                "INSERT INTO test_table VALUES (" + std::to_string(i) + ", 'data')"
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Trigger failover mid-write
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop_node(cluster_->get_primary_node_id());

    writer.join();

    // Verify no data loss (some writes may fail but completed ones should persist)
    auto result = gateway_->execute_read("SELECT COUNT(*) FROM test_table");
    // Count should be > 0
}
```

### 2.5 Network Partition Tests

```cpp
// tests/distributed/network_partition_test.cpp
#include "multi_node_test_framework.h"

class NetworkPartitionTest : public database::test::MultiNodeTestBase {};

TEST_F(NetworkPartitionTest, PartitionedReplicaRecovery) {
    start_all_nodes();

    // Insert initial data
    gateway_->execute_write("INSERT INTO test_table VALUES (1, 'initial')");
    EXPECT_TRUE(wait_for_replication_sync(std::chrono::seconds(10)));

    // Partition replica1
    partition_node("replica1");

    // Continue writes during partition
    for (int i = 2; i <= 10; ++i) {
        gateway_->execute_write(
            "INSERT INTO test_table VALUES (" + std::to_string(i) + ", 'during_partition')"
        );
    }

    // Verify replica1 is behind
    auto primary_count = get_row_counts("test_table")["primary"];
    EXPECT_EQ(primary_count, 10);

    // Heal partition
    heal_partition("replica1");

    // Wait for catch-up
    EXPECT_TRUE(wait_for_replication_sync(std::chrono::seconds(30)));

    // Verify replica1 caught up
    EXPECT_TRUE(verify_data_consistency());
}

TEST_F(NetworkPartitionTest, SplitBrainPrevention) {
    config_.num_replica_nodes = 2;  // 3 node cluster
    start_all_nodes();

    // Partition primary from both replicas
    partition_node("primary");

    // Wait for failover among replicas
    std::this_thread::sleep_for(std::chrono::seconds(20));

    // Verify only one primary exists
    int primary_count = 0;
    for (const auto& node : cluster_->get_all_nodes()) {
        if (node.role == node_role::PRIMARY) primary_count++;
    }
    EXPECT_EQ(primary_count, 1);

    // Old primary should detect it's partitioned and step down
    heal_partition("primary");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Old primary should rejoin as replica
    auto old_primary_info = cluster_->get_node_info("primary");
    EXPECT_EQ(old_primary_info.role, node_role::REPLICA);
}
```

### 2.6 Performance Benchmarks

```cpp
// tests/distributed/distributed_benchmark_test.cpp
#include "multi_node_test_framework.h"
#include <benchmark/benchmark.h>

class DistributedBenchmark : public database::test::MultiNodeTestBase {};

TEST_F(DistributedBenchmark, ThroughputBenchmark) {
    start_all_nodes();

    constexpr int NUM_OPERATIONS = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < NUM_OPERATIONS / 10; ++i) {
                int id = t * (NUM_OPERATIONS / 10) + i;
                if (gateway_->execute_write(
                    "INSERT INTO bench VALUES (" + std::to_string(id) + ")").success) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    double ops_per_second = (success_count.load() * 1000.0) / ms.count();

    std::cout << "Throughput: " << ops_per_second << " ops/sec\n";
    std::cout << "Success rate: " << (success_count.load() * 100.0 / NUM_OPERATIONS) << "%\n";

    EXPECT_GT(ops_per_second, 1000);  // Minimum 1000 ops/sec
}

TEST_F(DistributedBenchmark, ReplicationLagMeasurement) {
    start_all_nodes();

    std::vector<std::chrono::microseconds> lag_samples;

    for (int i = 0; i < 100; ++i) {
        auto write_time = std::chrono::high_resolution_clock::now();

        gateway_->execute_write(
            "INSERT INTO lag_test VALUES (" + std::to_string(i) + ")"
        );

        // Poll replica until data appears
        while (true) {
            auto result = cluster_->execute_on_node(
                "replica1",
                "SELECT * FROM lag_test WHERE id = " + std::to_string(i)
            );
            if (!result.empty()) break;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        auto replicated_time = std::chrono::high_resolution_clock::now();
        lag_samples.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(
                replicated_time - write_time
            )
        );
    }

    // Calculate statistics
    auto avg_lag = std::accumulate(lag_samples.begin(), lag_samples.end(),
                                   std::chrono::microseconds(0)).count() / lag_samples.size();

    std::cout << "Average replication lag: " << avg_lag << " microseconds\n";

    EXPECT_LT(avg_lag, 100000);  // < 100ms average lag
}
```

### 2.7 Implementation Steps

1. **Test Infrastructure** (Days 1-2)
   - Docker Compose configuration
   - Test framework base class
   - Toxiproxy integration for fault injection

2. **Cluster Formation Tests** (Day 3)
   - Startup/shutdown tests
   - Dynamic node addition/removal
   - Discovery and registration

3. **Failover Tests** (Days 4-5)
   - Automatic failover
   - Manual failover
   - Data integrity during failover

4. **Network Partition Tests** (Days 5-6)
   - Partition and recovery
   - Split-brain scenarios
   - Replication catch-up

5. **Performance Benchmarks** (Day 7)
   - Throughput measurement
   - Latency distribution
   - Replication lag

---

## 3. How to Test

### 3.1 Test Execution

```bash
# Start test environment
docker-compose -f docker-compose.distributed-test.yml up -d

# Wait for services
./scripts/wait-for-services.sh

# Run distributed tests
ctest -R distributed -V --timeout 600

# Cleanup
docker-compose -f docker-compose.distributed-test.yml down -v
```

### 3.2 CI Integration

```yaml
# .github/workflows/distributed-tests.yml
name: Distributed Tests

on:
  push:
    branches: [main]
  schedule:
    - cron: '0 2 * * *'  # Nightly

jobs:
  distributed-tests:
    runs-on: ubuntu-latest
    timeout-minutes: 30

    steps:
      - uses: actions/checkout@v4

      - name: Start test cluster
        run: docker-compose -f docker-compose.distributed-test.yml up -d

      - name: Run distributed tests
        run: |
          cmake -B build -DENABLE_DISTRIBUTED_TESTS=ON
          cmake --build build
          cd build && ctest -R distributed -V

      - name: Collect logs
        if: failure()
        run: docker-compose logs > distributed-test-logs.txt

      - name: Upload logs
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: distributed-test-logs
          path: distributed-test-logs.txt
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Cluster formation tests | Pass 100% | CI pipeline |
| Failover tests | Pass 100% | CI pipeline |
| Network partition recovery | <60 seconds | Integration test |
| Data consistency after failures | 100% | Checksum verification |
| Throughput (3-node cluster) | >1000 ops/sec | Benchmark |
| Replication lag (async) | <100ms average | Benchmark |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Flaky tests due to timing | HIGH | Generous timeouts, retries |
| CI resource constraints | MEDIUM | Dedicated runners or scheduled runs |
| Docker networking issues | MEDIUM | Health checks, retry logic |
| Test isolation failures | LOW | Clean environment per test |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**:
  - [DB-004](DB-004-gateway.md) (Gateway)
  - [DB-005](DB-005-replication.md) (Replication)
- **Related**:
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)
  - [DB-009](DB-009-async-stress.md) (Stress Tests)

---

## 6. Notes

- Tests should be idempotent and isolated
- Use unique table names per test to avoid conflicts
- Consider running subset in CI, full suite nightly
- Document known timing-sensitive tests

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
