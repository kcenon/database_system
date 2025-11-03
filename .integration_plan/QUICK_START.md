# Quick Start Guide

> **⚠️ FOR DEVELOPERS STARTING THE INTEGRATION WORK**

This is a quick reference for getting started with the database system integration plan.

## 🚀 Getting Started (5 minutes)

### 1. Read These Documents First

**Priority 1** (Must read):
1. `README.md` - Overview and structure
2. `00_MASTER_PLAN.md` - Complete phases and timeline
3. `01_ARCHITECTURE.md` - Design and patterns

**Priority 2** (Read as needed):
4. `09_TESTING_PLAN.md` - Testing strategy
5. Phase-specific checklists in `checklists/`

### 2. Set Up Development Environment

```bash
# Clone or navigate to database_system
cd /Users/dongcheolshin/Sources/database_system

# Ensure external systems are available (optional)
ls /Users/dongcheolshin/Sources/common_system
ls /Users/dongcheolshin/Sources/thread_system
ls /Users/dongcheolshin/Sources/logger_system
ls /Users/dongcheolshin/Sources/monitoring_system

# Create feature branch
git checkout -b feature/adapter-integration

# Build current state (verify everything works)
./build.sh --clean
```

### 3. Start Phase 1

```bash
# Go to Phase 1 checklist
open .integration_plan/checklists/phase1_checklist.md

# Follow the checklist step by step
# Create directories
mkdir -p database/integrated/core
mkdir -p database/integrated/adapters

# Create first file: configuration.h
# (See phase1_checklist.md for details)
```

---

## 📚 Document Hierarchy

```
.integration_plan/
├── README.md                     ← Start here (you are here)
├── QUICK_START.md               ← This file
├── 00_MASTER_PLAN.md            ← Read second: Full overview
├── 01_ARCHITECTURE.md           ← Read third: Design details
├── 02-08_*.md                   ← Read as needed for each phase
├── 09_TESTING_PLAN.md           ← Read before writing tests
├── 10_CMAKE_INTEGRATION.md      ← Read when updating CMake
└── checklists/
    ├── phase1_checklist.md      ← START HERE for implementation
    ├── phase2_checklist.md      ← Next: Logger adapter
    ├── phase3_checklist.md      ← Then: Monitoring adapter
    └── ...                       ← Continue sequentially
```

---

## 🎯 Implementation Workflow

### Daily Workflow

1. **Morning**:
   - Open current phase checklist
   - Review tasks for the day
   - Update status in checklist

2. **During Work**:
   - Check off tasks as you complete them
   - Run tests frequently
   - Commit small, logical changes

3. **End of Day**:
   - Update checklist with progress
   - Note any blockers or questions
   - Commit work-in-progress to feature branch

### Moving Between Phases

1. **Before Starting New Phase**:
   - [ ] Ensure previous phase "Definition of Done" is met
   - [ ] All tests pass
   - [ ] Code reviewed (self-review minimum)
   - [ ] Documentation updated

2. **Starting New Phase**:
   - [ ] Read phase document (e.g., `03_LOGGER_ADAPTER.md`)
   - [ ] Open phase checklist (e.g., `phase2_checklist.md`)
   - [ ] Understand dependencies
   - [ ] Set up test harness

---

## 💡 Key Concepts

### Adapter Pattern

Each system (logger, monitoring, thread) has an adapter:
- **Interface**: Public API (`.h` file)
- **Implementation**: PIMPL with conditional compilation (`.cpp` file)
- **Fallback**: Works without external system

```cpp
#if defined(USE_LOGGER_SYSTEM)
    // Use logger_system
#else
    // Use std::cout + std::ofstream
#endif
```

### PIMPL Idiom

```cpp
// Header (.h)
class logger_adapter {
public:
    // Public interface
private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

// Implementation (.cpp)
class logger_adapter::impl {
    // Private implementation details
};
```

### Builder Pattern

```cpp
auto config = unified_db_config{}
    .set_backend(backend_type::postgres, "...")
    .set_pool_size(10, 50)
    .set_log_level(db_log_level::debug);
```

---

## 🔧 Common Commands

### Build Commands

```bash
# Clean build with all systems
cmake -DUSE_LOGGER_SYSTEM=ON \
      -DUSE_MONITORING_SYSTEM=ON \
      -DUSE_THREAD_SYSTEM=ON \
      -DUSE_COMMON_SYSTEM=ON \
      -S . -B build

cmake --build build

# Build without external systems (fallback mode)
cmake -DUSE_LOGGER_SYSTEM=OFF \
      -DUSE_MONITORING_SYSTEM=OFF \
      -S . -B build-fallback

cmake --build build-fallback
```

### Test Commands

```bash
# Run all tests
./build/bin/all_tests

# Run specific test
./build/bin/test_logger_adapter

# Run with verbose output
./build/bin/all_tests --gtest_filter="*" -V
```

### Static Analysis

```bash
# clang-tidy
clang-tidy database/integrated/adapters/*.cpp \
    -p=build/compile_commands.json

# cppcheck
cppcheck --enable=all \
         --suppress=missingIncludeSystem \
         database/integrated/
```

### Memory Testing

```bash
# AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ...
./build/bin/all_tests

# ThreadSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ...
./build/bin/all_tests
```

---

## 🆘 Troubleshooting

### "Cannot find logger_system"

**Solution**: Either install logger_system or build with `USE_LOGGER_SYSTEM=OFF`

```bash
cmake -DUSE_LOGGER_SYSTEM=OFF -S . -B build
```

### "Tests failing"

**Checklist**:
1. Did you run `cmake` after changing CMakeLists.txt?
2. Did you rebuild after changing source code?
3. Are all external systems available?
4. Check test output for specific error

### "Compile errors in adapter"

**Common causes**:
1. Missing include path to external system
2. Typo in conditional compilation (`#if defined(...)`)
3. PIMPL forward declaration mismatch

---

## 📞 Getting Help

### Questions About...

- **Architecture**: See `01_ARCHITECTURE.md`
- **Testing**: See `09_TESTING_PLAN.md`
- **Specific Phase**: See corresponding `0N_*.md` document
- **Implementation Details**: Check phase checklist

### Stuck on a Task?

1. Check if prerequisite tasks are complete
2. Review the phase document
3. Look at integrated_thread_system reference implementation
4. Add note in checklist and move to next task if blocked

---

## ✅ Success Indicators

You're on the right track if:
- [ ] Tests are passing
- [ ] No compiler warnings
- [ ] Code follows existing style
- [ ] Documentation is clear
- [ ] Checklist items are being completed
- [ ] Commits are small and logical

---

## 🎓 Learning Resources

### Reference Implementation

Study `integrated_thread_system` for patterns:

```bash
cd /Users/dongcheolshin/Sources/integrated_thread_system

# Study adapter pattern
cat include/kcenon/integrated/adapters/logger_adapter.h
cat src/adapters/logger_adapter.cpp

# Study coordinator
cat include/kcenon/integrated/core/system_coordinator.h

# Study configuration
cat include/kcenon/integrated/core/configuration.h
```

### Key Files to Reference

1. **integrated_thread_system**:
   - `include/kcenon/integrated/adapters/logger_adapter.h`
   - `src/adapters/logger_adapter.cpp`
   - `include/kcenon/integrated/core/system_coordinator.h`

2. **database_system** (existing):
   - `database/adapters/common_system_adapter.h`
   - `database/pooling/connection_pool_v2.h`

---

**Good Luck! 🚀**

Start with Phase 1 checklist and work through sequentially.
Remember: this directory will be deleted when integration is complete!
