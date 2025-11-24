#!/bin/bash
# Wait for all database services to be ready

set -e

echo "Waiting for database services to be ready..."

# Wait for PostgreSQL Primary
echo -n "Waiting for postgres-primary... "
for i in {1..30}; do
    if docker exec db-postgres-primary pg_isready -U test -d testdb &>/dev/null; then
        echo "ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "timeout"
        exit 1
    fi
    sleep 1
done

# Wait for PostgreSQL Replica 1
echo -n "Waiting for postgres-replica1... "
for i in {1..30}; do
    if docker exec db-postgres-replica1 pg_isready -U test -d testdb &>/dev/null; then
        echo "ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "timeout"
        exit 1
    fi
    sleep 1
done

# Wait for PostgreSQL Replica 2
echo -n "Waiting for postgres-replica2... "
for i in {1..30}; do
    if docker exec db-postgres-replica2 pg_isready -U test -d testdb &>/dev/null; then
        echo "ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "timeout"
        exit 1
    fi
    sleep 1
done

# Wait for MySQL
echo -n "Waiting for mysql-node... "
for i in {1..30}; do
    if docker exec db-mysql-node mysqladmin ping -h localhost -u test -ptest &>/dev/null; then
        echo "ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "timeout"
        exit 1
    fi
    sleep 1
done

# Wait for Toxiproxy
echo -n "Waiting for toxiproxy... "
for i in {1..30}; do
    if curl -s http://localhost:8474/version &>/dev/null; then
        echo "ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "timeout"
        exit 1
    fi
    sleep 1
done

echo "All services are ready!"

# Initialize Toxiproxy proxies
echo "Initializing Toxiproxy proxies..."

curl -s -X POST http://localhost:8474/proxies \
  -d '{"name":"postgres-primary","listen":"0.0.0.0:5435","upstream":"postgres-primary:5432","enabled":true}' \
  -H "Content-Type: application/json" || true

curl -s -X POST http://localhost:8474/proxies \
  -d '{"name":"postgres-replica1","listen":"0.0.0.0:5436","upstream":"postgres-replica1:5432","enabled":true}' \
  -H "Content-Type: application/json" || true

curl -s -X POST http://localhost:8474/proxies \
  -d '{"name":"postgres-replica2","listen":"0.0.0.0:5437","upstream":"postgres-replica2:5432","enabled":true}' \
  -H "Content-Type: application/json" || true

curl -s -X POST http://localhost:8474/proxies \
  -d '{"name":"mysql-node","listen":"0.0.0.0:3307","upstream":"mysql-node:3306","enabled":true}' \
  -H "Content-Type: application/json" || true

echo "Toxiproxy proxies configured!"
echo "Setup complete!"
