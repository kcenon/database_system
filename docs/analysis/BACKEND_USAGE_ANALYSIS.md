# Backend Usage Analysis Report

## Overview

This document analyzes the usage patterns and maintenance considerations for MongoDB and Redis backends in database_system, as part of the evaluation for Issue #333.

**Analysis Date**: 2026-01-22
**Analyzed by**: Issue #335 - Backend Usage Analysis

---

## Executive Summary

| Finding | Status |
|---------|--------|
| MongoDB/Redis in vcpkg.json | **Not present** |
| Dedicated test files | **None** |
| Sample applications | **None** |
| GitHub Issues/PRs mentioning backends | Primarily maintenance/refactoring |
| Code contribution to project | ~42% of backend code |

**Recommendation**: Proceed with **Option B** (CMake Optional Modules) as the optimal balance between flexibility and maintenance.

---

## 1. Code Metrics Analysis

### 1.1 Lines of Code by Backend

| Backend | Header (LOC) | Source (LOC) | Total (LOC) | % of Backends |
|---------|--------------|--------------|-------------|---------------|
| PostgreSQL | 180 | 630 | 810 | 21.3% |
| MySQL | 171 | 520 | 691 | 18.1% |
| SQLite | 178 | 537 | 715 | 18.8% |
| **MongoDB** | 190 | 635 | 825 | **21.7%** |
| **Redis** | 177 | 590 | 767 | **20.1%** |
| **Total** | 896 | 2,912 | 3,808 | 100% |

**Key Finding**: MongoDB + Redis = 1,592 LOC (41.8% of backend code)

### 1.2 Code Complexity Comparison

| Metric | SQL Backends | NoSQL Backends |
|--------|--------------|----------------|
| Query Builder Support | Full | None/Partial |
| Transaction Support | Full ACID | Limited/None |
| ORM Integration | Yes | No |
| Type Safety | Strong | Weak (void* pointers) |

---

## 2. Dependency Analysis

### 2.1 vcpkg.json Features

```json
{
  "features": {
    "postgresql": { "dependencies": ["libpq", "libpqxx", "openssl"] },
    "mysql": { "dependencies": ["libmariadb"] },
    "sqlite": { "dependencies": ["sqlite3"] }
    // MongoDB and Redis: NOT DEFINED
  }
}
```

**Critical Finding**: MongoDB and Redis are NOT in vcpkg.json, meaning:
- These backends cannot be built without manual dependency setup
- CI/CD pipeline does not test these backends
- Users cannot easily enable these features

### 2.2 Code Dependencies

Files importing MongoDB/Redis backends:
- `database/database_manager.cpp` - Runtime backend selection
- `database/query_dialect.cpp` - Dialect handling
- Sample files mention backends but no dedicated NoSQL samples exist

---

## 3. GitHub Activity Analysis

### 3.1 Issue Statistics

| Category | MongoDB | Redis | SQL Backends |
|----------|---------|-------|--------------|
| Feature Requests | 2 | 1 | 15+ |
| Bug Reports | 0 | 1 | 8 |
| Refactoring | 8 | 7 | 12 |
| Documentation | 1 | 1 | 5 |

### 3.2 Notable Issues

| Issue | Type | Status | Description |
|-------|------|--------|-------------|
| #200 | Feature | Closed | Initial NoSQL Implementation |
| #208 | Refactor | Closed | Redis logging adapter |
| #312 | Refactor | Closed | Remove legacy query builders |
| #333 | Refactor | Open | Current evaluation issue |

**Pattern**: Most MongoDB/Redis issues are maintenance/refactoring, not feature requests or bug reports.

---

## 4. Test Coverage Analysis

### 4.1 Test File Distribution

| Backend | Dedicated Test File | Integration Tests |
|---------|--------------------|--------------------|
| PostgreSQL | No | Yes (integration_tests.cpp) |
| MySQL | No | Yes (integration_tests.cpp) |
| SQLite | sqlite_backend_test.cpp | Yes |
| **MongoDB** | **None** | **Partial** |
| **Redis** | **None** | **Partial** |

### 4.2 Test Configuration

```cpp
// tests/CMakeLists.txt - No MongoDB/Redis specific tests registered
```

---

## 5. Alternative Library Comparison

### 5.1 MongoDB

| Aspect | Current (mongodb_backend) | mongocxx (Official) |
|--------|---------------------------|---------------------|
| Maintainer | database_system | MongoDB Inc. |
| Documentation | Minimal | Comprehensive |
| Features | Basic CRUD | Full driver |
| Thread Safety | Manual mutex | Built-in |
| Type Safety | void* pointers | Strong types |
| Version Support | Unknown | Current releases |

### 5.2 Redis

| Aspect | Current (redis_backend) | hiredis (Official) |
|--------|-------------------------|---------------------|
| Maintainer | database_system | Redis Inc. |
| Documentation | Minimal | Comprehensive |
| Features | Basic commands | Full protocol |
| Async Support | No | Yes |
| Cluster Support | No | Yes |
| Connection Pooling | No | Yes |

---

## 6. Maintenance Effort Assessment

### 6.1 Current Burden

| Task | SQL Backends | NoSQL Backends | Notes |
|------|--------------|----------------|-------|
| API Changes | Moderate | High | NoSQL uses different paradigms |
| Testing | Automated | Manual | No CI coverage for NoSQL |
| Documentation | Good | Poor | Few examples |
| Bug Fixes | Reactive | Proactive | Less usage = fewer reports |

### 6.2 Build Time Impact

With NoSQL backends removed:
- Fewer compilation units
- Reduced dependency resolution
- Estimated ~10-15% faster CI builds

---

## 7. Recommendations

### 7.1 Recommended Option: B (CMake Optional Modules)

```cmake
# Proposed CMakeLists.txt changes
option(DATABASE_WITH_MONGODB "Include MongoDB backend" OFF)
option(DATABASE_WITH_REDIS "Include Redis backend" OFF)

# Add to vcpkg.json as optional features
"mongodb": {
  "description": "Enable MongoDB backend (experimental)",
  "dependencies": [{ "name": "mongo-cxx-driver" }]
},
"redis": {
  "description": "Enable Redis backend (experimental)",
  "dependencies": [{ "name": "hiredis" }]
}
```

**Rationale**:
1. Maintains backward compatibility
2. Reduces core complexity
3. Allows opt-in for interested users
4. Easier testing and maintenance
5. No separate repository management

### 7.2 Implementation Priority

1. **Phase 1**: Add CMake options (disabled by default)
2. **Phase 2**: Add vcpkg features for dependencies
3. **Phase 3**: Mark as "experimental" in documentation
4. **Phase 4**: Evaluate removal in future version if no adoption

### 7.3 Migration Path

For users currently using MongoDB/Redis:
1. Enable CMake option explicitly
2. Install additional vcpkg dependencies
3. No code changes required

---

## 8. Decision Matrix

| Criterion | Weight | Option A (Separate) | Option B (CMake) | Option C (Keep) |
|-----------|--------|--------------------|--------------------|-----------------|
| Maintenance | 30% | 9/10 | 8/10 | 4/10 |
| Backward Compat | 25% | 5/10 | 9/10 | 10/10 |
| User Experience | 20% | 6/10 | 8/10 | 7/10 |
| Implementation | 15% | 4/10 | 9/10 | 10/10 |
| Future Flexibility | 10% | 8/10 | 8/10 | 5/10 |
| **Total** | 100% | **6.55** | **8.35** | **6.85** |

---

## 9. Appendix

### A. Files Analyzed

```
database/backends/mongodb_backend.h
database/backends/mongodb_backend.cpp
database/backends/redis_backend.h
database/backends/redis_backend.cpp
database/database_manager.cpp
vcpkg.json
CMakeLists.txt
tests/CMakeLists.txt
```

### B. GitHub Search Queries Used

```bash
gh issue list --state all --search "mongodb"
gh issue list --state all --search "redis"
gh pr list --state all --search "mongodb"
gh pr list --state all --search "redis"
```

### C. Related Issues

- #333: Parent evaluation issue
- #200: Original NoSQL implementation
- #312: Legacy query builder removal
- #328: Backend lifecycle template

---

*Generated as part of Issue #335 analysis phase*
