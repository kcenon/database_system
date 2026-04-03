---
doc_id: "DBS-PERF-007"
doc_title: "정적 분석 베이스라인 - database_system"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "PERF"
---

# 정적 분석 베이스라인 - database_system

> **Language:** [English](STATIC_ANALYSIS_BASELINE.md) | **한국어**

**날짜**: 2025-10-03
**버전**: 0.1.0.0
**도구 버전**:
- clang-tidy: 18.x
- cppcheck: 2.x

## 개요

이 문서는 database_system의 정적 분석 경고 베이스라인을 설정합니다.
목표는 시간이 지남에 따라 개선을 추적하고 회귀를 방지하는 것입니다.

## Clang-Tidy 베이스라인

### 구성
- 활성화된 검사: modernize-*, concurrency-*, performance-*, bugprone-*, cert-*, cppcoreguidelines-*
- 표준: C++20
- 구성 파일: .clang-tidy

### 초기 베이스라인 (Phase 0)

**총 경고 수**: TBD
실행: `clang-tidy -p build/compile_commands.json <source_files>`

**경고 분포**:
- modernize-*: TBD
- performance-*: TBD
- concurrency-*: TBD
- readability-*: TBD
- bugprone-*: TBD

### 주목할 만한 억제 항목
전체 억제된 검사 목록은 .clang-tidy를 참조하세요.

## Cppcheck 베이스라인

### 구성
- 프로젝트 파일: .cppcheck
- 활성화: 모든 검사
- 표준: C++20

### 초기 베이스라인 (Phase 0)

**총 이슈 수**: TBD
실행: `cppcheck --project=.cppcheck --enable=all`

**이슈 분포**:
- Error: TBD
- Warning: TBD
- Style: TBD
- Performance: TBD

### 주목할 만한 억제 항목
전체 억제 목록은 .cppcheck를 참조하세요.

## 목표

**Phase 1 목표** (2025-11-01까지):
- clang-tidy: 0 에러, < 20 경고
- cppcheck: 0 에러, < 10 경고

**Phase 2 목표** (2025-12-01까지):
- clang-tidy: < 10 경고
- cppcheck: < 5 경고

**Phase 3 목표** (2026-01-01까지):
- 모든 경고 해결 또는 명시적으로 문서화

## 분석 실행 방법

### Clang-Tidy
```bash
# Generate compile commands
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build <source_files>

# Or check all files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-tidy -p build
```

### Cppcheck
```bash
# Using project configuration
cppcheck --project=.cppcheck --enable=all --xml 2> cppcheck.xml

# Generate HTML report
cppcheck-htmlreport --file=cppcheck.xml --report-dir=build/cppcheck-report
```

## 변경사항 추적

경고 증가는 정당한 사유와 함께 여기에 문서화해야 합니다:

| 날짜 | 도구 | 변경사항 | 사유 | 해결 여부 |
|------|------|----------|------|-----------|
| 2025-10-03 | clang-tidy | 초기 베이스라인 | Phase 0 설정 | N/A |
| 2025-10-03 | cppcheck | 초기 베이스라인 | Phase 0 설정 | N/A |

## 참고사항

- 베이스라인은 Phase 1에서 초기 경고 수정 후 업데이트됩니다
- 목표는 지속적인 개선과 회귀 제로입니다
- 모든 새 코드는 정적 분석 검사를 통과해야 합니다

---

*Last Updated: 2025-10-20*
