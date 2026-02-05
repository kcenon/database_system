# Database System Documentation

> **Language:** **English** | [한국어](README.kr.md)

**Version:** 0.1.0.0
**Last Updated:** 2025-11-11
**Status:** Comprehensive

Welcome to the database_system documentation! This unified database abstraction layer supports 5 backends (PostgreSQL, MySQL, SQLite, MongoDB, Redis) with enterprise-grade connection pooling and advanced query builders.

---

## 🚀 Quick Navigation

| I want to... | Document |
|--------------|----------|
| ⚡ Get started in 5 minutes | [Quick Start](guides/QUICK_START.md) |
| 🏗️ Understand the architecture | [Architecture](01-ARCHITECTURE.md) |
| 📖 Look up an API | [API Reference](02-API_REFERENCE.md) |
| 🔧 Build from source | [Build Guide](guides/BUILD_GUIDE.md) |
| ❓ Find answers to common questions | [FAQ](guides/FAQ.md) (20+ Q&A) |
| 🐛 Troubleshoot an issue | [Troubleshooting](guides/TROUBLESHOOTING.md) |
| ✨ Learn best practices | [Best Practices](guides/BEST_PRACTICES.md) |
| 🤝 Contribute to the project | [Contributing](contributing/CONTRIBUTING.md) |

---

## Table of Contents

- [Documentation Structure](#documentation-structure)
- [Documentation by Role](#documentation-by-role)
- [By Feature](#by-feature)
- [By Database Type](#by-database-type)
- [Contributing to Documentation](#contributing-to-documentation)

---

## Documentation Structure

### 📘 Core Documentation

Essential documents for understanding the system:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [01-ARCHITECTURE.md](01-ARCHITECTURE.md) | System architecture, design decisions, backend plugin system | [🇰🇷](01-ARCHITECTURE.kr.md) | 600+ |
| [02-API_REFERENCE.md](02-API_REFERENCE.md) | Complete API docs: database_manager, connection_pool, ORM, query builders | [🇰🇷](02-API_REFERENCE.kr.md) | 1000+ |

### 📗 User Guides

Step-by-step guides for users:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [QUICK_START.md](guides/QUICK_START.md) | 5-minute getting started guide | - | 170 |
| [BUILD_GUIDE.md](guides/BUILD_GUIDE.md) | Build instructions, dependencies, troubleshooting | [🇰🇷](guides/BUILD_GUIDE.kr.md) | 500+ |
| [FAQ.md](guides/FAQ.md) | 20+ frequently asked questions with answers | - | 484 |
| [TROUBLESHOOTING.md](guides/TROUBLESHOOTING.md) | Common problems and solutions | - | 964 |
| [BEST_PRACTICES.md](guides/BEST_PRACTICES.md) | Recommended patterns for connection, queries, security | - | 1255 |
| [SAMPLES_GUIDE.md](guides/SAMPLES_GUIDE.md) | Walkthrough of example applications | [🇰🇷](guides/SAMPLES_GUIDE.kr.md) | 800+ |

### 📙 Advanced Topics

For experienced users and contributors:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [ADAPTER_PATTERNS.md](ADAPTER_PATTERNS.md) | Adapter pattern best practices for dependency management | [🇰🇷](ADAPTER_PATTERNS.kr.md) | 500+ |
| [TYPE_SYSTEM.md](advanced/TYPE_SYSTEM.md) | database_value, type mapping, ORM integration | - | 484 |
| [THREAD_SYSTEM_MIGRATION.md](advanced/THREAD_SYSTEM_MIGRATION.md) | Thread system integration guide | - | 300+ |
| [CURRENT_STATE.md](advanced/CURRENT_STATE.md) | Current implementation status | [🇰🇷](advanced/CURRENT_STATE.kr.md) | 100+ |
| [ARCHITECTURE_ISSUES.md](advanced/ARCHITECTURE_ISSUES.md) | Known architectural issues | [🇰🇷](advanced/ARCHITECTURE_ISSUES.kr.md) | 50+ |

### 📊 Performance

Performance metrics and optimization:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [BASELINE.md](performance/BASELINE.md) | Performance baseline: 1.2ms queries, 5K TPS, 1.16M ops/s | [🇰🇷](performance/BASELINE.kr.md) | 300+ |
| [BENCHMARKS.md](performance/BENCHMARKS.md) | Detailed benchmark results by backend | [🇰🇷](performance/BENCHMARKS.kr.md) | 600+ |
| [STATIC_ANALYSIS_BASELINE.md](performance/STATIC_ANALYSIS_BASELINE.md) | Static analysis results (Clang-Tidy, Cppcheck) | [🇰🇷](performance/STATIC_ANALYSIS_BASELINE.kr.md) | 100+ |

### 🤝 Contributing

For contributors and maintainers:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [CONTRIBUTING.md](contributing/CONTRIBUTING.md) | Contribution guidelines, code style, testing | - | 955 |
| [CI_CD_GUIDE.md](contributing/CI_CD_GUIDE.md) | CI/CD pipeline, sanitizers, benchmarks | - | 530 |

---

## Documentation by Role

### 👤 For New Users

**Getting Started Path**:
1. **⚡ Quick Start** - [5-minute guide](guides/QUICK_START.md) to first program
2. **🏗️ Architecture** - [System overview](01-ARCHITECTURE.md) and design
3. **📖 API Reference** - [Complete API](02-API_REFERENCE.md) documentation
4. **💡 Examples** - [Samples guide](guides/SAMPLES_GUIDE.md) with walkthroughs

**When You Have Issues**:
- Check [FAQ](guides/FAQ.md) first (20+ common questions)
- Use [Troubleshooting](guides/TROUBLESHOOTING.md) for problems
- Search [GitHub Issues](https://github.com/kcenon/database_system/issues)

### 💻 For Experienced Developers

**Advanced Usage Path**:
1. **🏗️ Architecture** - Understand [backend plugin system](01-ARCHITECTURE.md)
2. **📖 API Reference** - Study [advanced APIs](02-API_REFERENCE.md)
3. **✨ Best Practices** - Learn [optimization patterns](guides/BEST_PRACTICES.md)
4. **📊 Performance** - Review [benchmarks](performance/BENCHMARKS.md)

**Deep Dive Topics**:
- [Adapter Patterns](ADAPTER_PATTERNS.md) - Optional dependency management
- [Type System](advanced/TYPE_SYSTEM.md) - Type mapping and ORM
- [Thread Integration](advanced/THREAD_SYSTEM_MIGRATION.md) - Multi-threading
- [Security Best Practices](guides/BEST_PRACTICES.md#security-best-practices)

### 🔧 For DevOps Engineers

**Deployment Path**:
1. **🔧 Build Guide** - [Build and install](guides/BUILD_GUIDE.md)
2. **📊 Benchmarks** - [Performance baselines](performance/BENCHMARKS.md)
3. **✨ Best Practices** - [Connection tuning](guides/BEST_PRACTICES.md#connection-management)
4. **🐛 Troubleshooting** - [Common issues](guides/TROUBLESHOOTING.md)

**Monitoring and Tuning**:
- [Connection Pool Performance](performance/BASELINE.md) - 0.1ms acquisition
- [Query Performance](performance/BENCHMARKS.md) - Backend-specific metrics
- [CI/CD Pipeline](contributing/CI_CD_GUIDE.md) - Automation

### 🤝 For Contributors

**Contribution Path**:
1. **🤝 Contributing** - [How to contribute](contributing/CONTRIBUTING.md)
2. **🔧 Build Guide** - [Development setup](guides/BUILD_GUIDE.md)
3. **🚀 CI/CD** - [Pipeline documentation](contributing/CI_CD_GUIDE.md)
4. **🏗️ Architecture** - [System internals](01-ARCHITECTURE.md)

**Development Resources**:
- [Code Style](contributing/CONTRIBUTING.md#code-style-guidelines)
- [Testing Guide](contributing/CI_CD_GUIDE.md#running-checks-locally)
- [Current Status](advanced/CURRENT_STATE.md) - Implementation status

---

## By Feature

### 🔗 Connection Management

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | database_manager |
| Pooling | [Best Practices](guides/BEST_PRACTICES.md) | Connection Management |
| Performance | [Benchmarks](performance/BENCHMARKS.md) | Connection Pool |
| Examples | [Samples Guide](guides/SAMPLES_GUIDE.md) | Connection Pool Demo |

### 🏊 Connection Pooling

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | connection_pool |
| Configuration | [FAQ](guides/FAQ.md) | Connection Pooling |
| Tuning | [Best Practices](guides/BEST_PRACTICES.md) | Performance Optimization |
| Benchmarks | [Baseline](performance/BASELINE.md) | 0.1ms acquisition |

### 🔍 Query Building

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | Query Builders |
| SQL Builder | [Samples Guide](guides/SAMPLES_GUIDE.md) | SQL Query Builder |
| MongoDB Builder | [Samples Guide](guides/SAMPLES_GUIDE.md) | MongoDB Query Builder |
| Redis Builder | [Samples Guide](guides/SAMPLES_GUIDE.md) | Redis Query Builder |

### 🗂️ ORM Framework

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | entity_manager |
| Type System | [Type System](advanced/TYPE_SYSTEM.md) | ORM Integration |
| Examples | [FAQ](guides/FAQ.md) | ORM Framework |
| Best Practices | [Best Practices](guides/BEST_PRACTICES.md) | Entity Mapping |

### 🔐 Security

| Topic | Document | Section |
|-------|----------|---------|
| Credentials | [Best Practices](guides/BEST_PRACTICES.md) | Security Best Practices |
| SQL Injection | [FAQ](guides/FAQ.md) | Security |
| Access Control | [Architecture](01-ARCHITECTURE.md) | RBAC |
| Audit Logging | [Best Practices](guides/BEST_PRACTICES.md) | Audit Logging |

---

## By Database Type

### 🐘 PostgreSQL

| Topic | Document |
|-------|----------|
| Setup | [Build Guide](guides/BUILD_GUIDE.md) - PostgreSQL dependencies |
| Examples | [Samples Guide](guides/SAMPLES_GUIDE.md) - PostgreSQL advanced |
| Performance | [Benchmarks](performance/BENCHMARKS.md) - 1.2ms SELECT |
| Tips | [Best Practices](guides/BEST_PRACTICES.md) - PostgreSQL-specific |

### 🐬 MySQL

| Topic | Document |
|-------|----------|
| Setup | [Build Guide](guides/BUILD_GUIDE.md) - MySQL dependencies |
| Query Builder | [Samples Guide](guides/SAMPLES_GUIDE.md) - SQL query builder |
| Performance | [Benchmarks](performance/BENCHMARKS.md) - MySQL metrics |
| Tips | [Best Practices](guides/BEST_PRACTICES.md) - MySQL-specific |

### 🗄️ SQLite

| Topic | Document |
|-------|----------|
| Setup | [Quick Start](guides/QUICK_START.md) - Easiest to start |
| Usage | [Samples Guide](guides/SAMPLES_GUIDE.md) - Local database |
| Performance | [Benchmarks](performance/BENCHMARKS.md) - 0.8ms SELECT |
| Tips | [Best Practices](guides/BEST_PRACTICES.md) - SQLite-specific |

### 🍃 MongoDB

| Topic | Document |
|-------|----------|
| Setup | [Build Guide](guides/BUILD_GUIDE.md) - MongoDB dependencies |
| Query Builder | [API Reference](02-API_REFERENCE.md) - mongodb_query_builder |
| Examples | [Samples Guide](guides/SAMPLES_GUIDE.md) - MongoDB examples |
| Tips | [Best Practices](guides/BEST_PRACTICES.md) - MongoDB-specific |

### 🔴 Redis

| Topic | Document |
|-------|----------|
| Setup | [Build Guide](guides/BUILD_GUIDE.md) - Redis dependencies |
| Query Builder | [API Reference](02-API_REFERENCE.md) - redis_query_builder |
| Examples | [Samples Guide](guides/SAMPLES_GUIDE.md) - Redis examples |
| Performance | [Benchmarks](performance/BENCHMARKS.md) - 0.3ms operations |

---

## Project Information

### Current Status
- **Version**: 0.1.0 (Phase 3 C++17 Migration Complete)
- **C++ Standard**: C++17 (C++20 for async/coroutines)
- **License**: BSD 3-Clause
- **Test Status**: 22/23 passing (95.7%)

### Supported Databases
- ✅ **PostgreSQL** - Full support with JSONB, CTEs, prepared statements
- ✅ **MySQL/MariaDB** - Complete implementation with utf8mb4
- ✅ **SQLite** - File and in-memory with WAL mode, FTS5
- ✅ **MongoDB** - Document operations and aggregation pipeline
- ✅ **Redis** - All data types with pipelining

### Key Features
- 🔗 **Multi-Backend** - Unified interface for 5 database types
- 🏊 **Connection Pooling** - 0.1ms acquisition, 10K+ connections
- 🔍 **Query Builders** - Type-safe SQL, MongoDB, Redis builders
- 🗂️ **ORM Framework** - Entity mapping with type-safe CRUD
- 🔐 **Security** - Credential encryption, RBAC, audit logging
- 🧵 **Thread Safe** - Concurrent operations verified with TSan
- 🛡️ **Production Ready** - Mock fallbacks, dependency injection

---

## Contributing to Documentation

### Documentation Standards
Follow the [Documentation Standard](/Users/raphaelshin/Sources/template_document/DOCUMENTATION_STANDARD.md):
- Front matter on all documents
- Code examples must compile
- Bilingual support (English/Korean)
- Cross-references with relative links

### Areas for Improvement
- [ ] Korean translations for new guides (FAQ, TROUBLESHOOTING, BEST_PRACTICES)
- [ ] Video tutorials
- [ ] Interactive examples
- [ ] More troubleshooting scenarios

### Submission Process
1. Read [Contributing Guide](contributing/CONTRIBUTING.md)
2. Edit markdown files
3. Test all code examples
4. Update Korean translations
5. Submit pull request

---

## 📞 Getting Help

### Documentation Issues
- **Missing info**: [Open documentation issue](https://github.com/kcenon/database_system/issues/new?labels=documentation)
- **Incorrect examples**: Report with details
- **Unclear instructions**: Suggest improvements

### Technical Support
1. Check [FAQ](guides/FAQ.md) - 20+ common questions
2. Read [Troubleshooting](guides/TROUBLESHOOTING.md) - Solutions to common problems
3. Search [GitHub Issues](https://github.com/kcenon/database_system/issues)
4. Ask on [GitHub Discussions](https://github.com/kcenon/database_system/discussions)

### Support Resources
- **Issues**: Bug reports and feature requests
- **Discussions**: Questions and support
- **Pull Requests**: Code and documentation contributions

---

## External Resources

- **GitHub Repository**: [kcenon/database_system](https://github.com/kcenon/database_system)
- **Issue Tracker**: [GitHub Issues](https://github.com/kcenon/database_system/issues)
- **Main README**: [../README.md](../README.md)
- **Improvement Plan**: [../IMPROVEMENT_PLAN.md](../IMPROVEMENT_PLAN.md)
- **Changelog**: [../CHANGELOG.md](../CHANGELOG.md)

---

## Documentation Roadmap

### ✅ Current (v1.0 - 2025-11-11)
- ✅ Complete API reference with examples
- ✅ Comprehensive build guide
- ✅ 20+ FAQ questions
- ✅ Detailed troubleshooting guide
- ✅ Best practices documentation
- ✅ Performance benchmarks
- ✅ CI/CD documentation
- ✅ Type system documentation

### 📋 Future Enhancements
- 📝 Korean translations for new guides
- 🎥 Video tutorials
- 📊 Interactive performance dashboard
- 🌐 Multi-language support (Japanese, Chinese)
- 📖 Migration guides for major versions

---

**Database System Documentation** - Enterprise-grade database abstraction for C++17/20

**Last Updated**: 2025-11-11
**Next Review**: 2026-02-11
