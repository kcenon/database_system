---
doc_id: "DBS-GUID-004"
doc_title: "Database System 문서"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Database System 문서

> **SSOT**: This document is the single source of truth for **Database System 문서**.

> **Language:** [English](README.md) | **한국어**

## 목차

- [📚 문서 개요](#문서-개요)
  - [📖 사용 가능한 문서](#사용-가능한-문서)
  - [🚀 빠른 시작](#빠른-시작)
- [📋 프로젝트 정보](#프로젝트-정보)
  - [현재 상태](#현재-상태)
  - [지원 데이터베이스](#지원-데이터베이스)
  - [주요 기능](#주요-기능)
- [📖 문서 구조](#문서-구조)
  - [핵심 문서](#핵심-문서)
    - [API Reference](#api-reference)
    - [Build Guide](#build-guide)
    - [Samples Guide](#samples-guide)
    - [Performance Benchmarks](#performance-benchmarks)
  - [추가 리소스](#추가-리소스)
    - [Changelog](#changelog)
    - [Project README](#project-readme)
- [🎯 사용 사례별 문서](#사용-사례별-문서)
  - [새 사용자](#새-사용자)
  - [숙련된 개발자](#숙련된-개발자)
  - [DevOps 및 시스템 관리자](#devops-및-시스템-관리자)
  - [학생 및 연구자](#학생-및-연구자)
- [🔍 정보 찾기](#정보-찾기)
  - [기능별](#기능별)
  - [데이터베이스 타입별](#데이터베이스-타입별)
- [🤝 문서 기여](#문서-기여)
  - [문서 표준](#문서-표준)
  - [개선 영역](#개선-영역)
  - [제출 프로세스](#제출-프로세스)
- [📞 도움 받기](#도움-받기)
  - [문서 이슈](#문서-이슈)
  - [기술 지원](#기술-지원)
  - [지원 리소스](#지원-리소스)
- [📅 문서 로드맵](#문서-로드맵)
  - [현재 (v3.0.0)](#현재-v300)
  - [향후 개선사항](#향후-개선사항)

멀티 백엔드 지원, 연결 풀링, 고급 쿼리 빌더를 갖춘 Database System - 엔터프라이즈급 C++20 데이터베이스 추상화 레이어의 포괄적인 문서에 오신 것을 환영합니다.

## 📚 문서 개요

이 문서는 Database System 프로젝트를 효과적으로 사용, 빌드 및 기여하는 데 필요한 모든 것을 제공합니다.

### 📖 사용 가능한 문서

| 문서 | 설명 | 대상 |
|------|------|------|
| **[API Reference](API_REFERENCE.kr.md)** | 예제가 포함된 완전한 API 문서 | 개발자 |
| **[Build Guide](BUILD_GUIDE.kr.md)** | 포괄적인 빌드 지침 및 문제 해결 | 개발자, DevOps |
| **[Samples Guide](SAMPLES_GUIDE.kr.md)** | 샘플 프로그램의 상세한 안내 | 개발자, 학생 |
| **[Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md)** | 성능 분석 및 최적화 가이드 | 아키텍트, DevOps |

### 🚀 빠른 시작

1. **개발자**: [메인 README](../README.md)부터 시작한 후 [API Reference](API_REFERENCE.kr.md)로 이동
2. **DevOps**: 배포 지침을 위해 [Build Guide](BUILD_GUIDE.kr.md) 확인
3. **학습용**: 실습 예제를 위해 [Samples Guide](SAMPLES_GUIDE.kr.md) 따라하기
4. **성능**: 최적화를 위해 [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md) 검토

## 📋 프로젝트 정보

### 현재 상태
- **최신 릴리스**: 2025년 1월 19일
- **C++ 표준**: C++20
- **라이선스**: BSD 3-Clause

### 지원 데이터베이스
- ✅ **PostgreSQL** - 고급 기능 완전 지원
- 제거됨: **MySQL/MariaDB** 백엔드는 Issue #418에서 삭제되었으며 현재 빌드에서는 사용할 수 없습니다.
- ✅ **SQLite** - 파일 및 인메모리 데이터베이스
- ✅ **MongoDB** - 문서 작업 및 집계
- ✅ **Redis** - 모든 데이터 타입 및 작업

### 주요 기능
- 🔗 **멀티 백엔드 지원** - SQL 및 NoSQL 데이터베이스를 위한 통합 인터페이스
- 🏊‍♂️ **연결 풀링** - 엔터프라이즈급 연결 관리
- 🔍 **쿼리 빌더** - 타입 안전 쿼리 구성
- 🧵 **스레드 안전성** - 적절한 동기화를 통한 동시 작업
- 🛡️ **개발 중** - Mock 대체, 에러 처리, 모니터링

## 📖 문서 구조

### 핵심 문서

#### [API Reference](API_REFERENCE.kr.md)
모든 클래스, 메서드 및 인터페이스에 대한 완전한 레퍼런스:
- 핵심 클래스 (`database_base`, `database_manager`)
- 연결 풀링 API (`connection_pool`, `connection_stats`)
- 쿼리 빌더 (`sql_query_builder`, `mongodb_query_builder`, `redis_query_builder`)
- 타입 시스템 (`database_types`, `database_value`)
- 포괄적인 코드 예제 및 사용 패턴

#### [Build Guide](BUILD_GUIDE.kr.md)
Database System을 빌드하고 배포하는 데 필요한 모든 것:
- 전제 조건 및 시스템 요구사항
- 플랫폼별 지침 (Linux, macOS, Windows)
- 데이터베이스 의존성 설치 (vcpkg, 수동)
- 빌드 구성 및 최적화
- 일반적인 문제 해결
- CI/CD 통합 예제

#### [Samples Guide](SAMPLES_GUIDE.kr.md)
샘플 프로그램의 상세한 탐색:
- 단계별 설명이 포함된 기본 사용 패턴
- 고급 PostgreSQL 기능 및 최적화
- 연결 풀링 데모
- 모든 데이터베이스 타입에 대한 쿼리 빌더 예제
- 다중 데이터베이스 사용 패턴
- 성능 최적화 기술

#### [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md)
포괄적인 성능 분석:
- 지연시간 및 처리량 측정
- 연결 풀 효율성 메트릭
- 쿼리 빌더 오버헤드 분석
- 메모리 사용량 프로파일링
- 확장성 테스트 결과
- 최적화 권장사항

### 추가 리소스

#### [Changelog](../CHANGELOG.md)
다음을 포함한 완전한 버전 히스토리:
- 기능 추가 및 개선사항
- 주요 변경사항 및 마이그레이션 가이드
- 버그 수정 및 개선사항
- 성능 최적화

#### [Project README](../README.md)
다음을 포함한 메인 프로젝트 문서:
- 프로젝트 개요 및 기능
- 빠른 시작 지침
- 사용 예제
- 기여 가이드라인

## 🎯 사용 사례별 문서

### 새 사용자
1. 개요를 위해 [Project README](../README.md)부터 시작
2. 시작하기 위해 [Build Guide](BUILD_GUIDE.kr.md) 따라하기
3. 실습 학습을 위해 [Samples Guide](SAMPLES_GUIDE.kr.md) 탐색
4. 필요에 따라 [API Reference](API_REFERENCE.kr.md) 참조

### 숙련된 개발자
1. 고급 기능을 위해 [API Reference](API_REFERENCE.kr.md) 검토
2. 최적화를 위해 [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md) 확인
3. 특정 패턴을 위해 [Samples Guide](SAMPLES_GUIDE.kr.md) 사용
4. 배포를 위해 [Build Guide](BUILD_GUIDE.kr.md) 참조

### DevOps 및 시스템 관리자
1. 배포 전략을 위해 [Build Guide](BUILD_GUIDE.kr.md)에 집중
2. 튜닝을 위해 [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md) 검토
3. 모니터링 설정을 위해 [API Reference](API_REFERENCE.kr.md) 사용
4. 버전 계획을 위해 [Changelog](../CHANGELOG.md) 확인

### 학생 및 연구자
1. 맥락을 위해 [Project README](../README.md)부터 시작
2. 학습을 위해 [Samples Guide](SAMPLES_GUIDE.kr.md) 작업
3. 분석을 위해 [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md) 연구
4. 구현 세부사항을 위해 [API Reference](API_REFERENCE.kr.md) 참조

## 🔍 정보 찾기

### 기능별

**연결 관리**
- API: [Database Manager](API_REFERENCE.kr.md#database-manager)
- 예제: [Basic Usage](guides/SAMPLES_GUIDE.kr.md)
- 빌드: [Database Dependencies](guides/BUILD_GUIDE.kr.md)

**연결 풀링**
- API: [Connection Pooling](API_REFERENCE.kr.md#연결-풀링)
- 예제: [Connection Pool Demo](guides/SAMPLES_GUIDE.kr.md)
- 성능: [Pool Performance](BENCHMARKS.kr.md#연결-풀-성능)

**쿼리 빌딩**
- API: [Query Builders](API_REFERENCE.kr.md#쿼리-빌더)
- 예제: [Query Builder Examples](guides/SAMPLES_GUIDE.kr.md)
- 성능: [Builder Performance](BENCHMARKS.kr.md#백엔드별-쿼리-성능)

**다중 데이터베이스 지원**
- API: [Database Types](API_REFERENCE.kr.md#데이터베이스-타입)
- 예제: [Multi-Database Examples](guides/SAMPLES_GUIDE.kr.md)
- 빌드: [Build Configurations](guides/BUILD_GUIDE.kr.md)

### 데이터베이스 타입별

**PostgreSQL**
- API: [postgres_manager](API_REFERENCE.kr.md#database_base)
- 예제: [PostgreSQL Advanced](guides/SAMPLES_GUIDE.kr.md)
- 성능: [PostgreSQL Benchmarks](BENCHMARKS.kr.md#postgresql-벤치마크)

**SQLite**
- 빌드: [SQLite Support](guides/BUILD_GUIDE.kr.md)
- 예제: [Local Database Usage](guides/SAMPLES_GUIDE.kr.md)
- 성능: [SQLite Benchmarks](BENCHMARKS.kr.md#sqlite-벤치마크)

**MongoDB**
- API: [mongodb_query_builder](API_REFERENCE.kr.md#mongodb_query_builder)
- 예제: [MongoDB Examples](guides/SAMPLES_GUIDE.kr.md)
- 성능: [MongoDB Performance](BENCHMARKS.kr.md#mongodb-벤치마크)

**Redis**
- API: [redis_query_builder](API_REFERENCE.kr.md#redis_query_builder)
- 예제: [Redis Examples](guides/SAMPLES_GUIDE.kr.md)
- 성능: [Redis Performance](BENCHMARKS.kr.md#redis-벤치마크)

## 🤝 문서 기여

문서 개선을 위한 기여를 환영합니다! 방법은 다음과 같습니다:

### 문서 표준
- 명확하고 간결한 언어 사용
- 모든 개념에 대한 실용적인 예제 포함
- 일관된 형식 및 구조 유지
- 제출 전 모든 코드 예제 테스트

### 개선 영역
- 추가 사용 예제
- 더 상세한 문제 해결 가이드
- 성능 최적화 팁
- 플랫폼별 지침

### 제출 프로세스
1. 저장소 포크
2. 문서 브랜치 생성
3. 개선사항 작성
4. 코드 예제 테스트
5. 명확한 설명과 함께 Pull Request 제출

## 📞 도움 받기

### 문서 이슈
- **누락된 정보**: 필요한 내용을 설명하는 이슈 생성
- **잘못된 예제**: 문제에 대한 세부 정보와 함께 보고
- **불명확한 지침**: 구체적인 개선사항 제안

### 기술 지원
- **빌드 문제**: [Build Guide](BUILD_GUIDE.kr.md) 문제 해결 섹션 확인
- **API 질문**: 먼저 [API Reference](API_REFERENCE.kr.md) 검토
- **성능 이슈**: [Performance Benchmarks](PERFORMANCE_BENCHMARKS.kr.md) 참조

### 지원 리소스
- **GitHub Issues**: 버그 보고 및 기능 요청
- **GitHub Discussions**: 질문 및 유지보수 지원
- **Pull Requests**: 코드 및 문서 기여

## 📅 문서 로드맵

### 현재 (v3.0.0)
- ✅ 예제가 포함된 완전한 API 레퍼런스
- ✅ 문제 해결이 포함된 포괄적인 빌드 가이드
- ✅ 안내가 포함된 상세한 샘플 가이드
- ✅ 실제 데이터를 포함한 성능 벤치마크

### 향후 개선사항
- 📋 대화형 API 문서
- 🎥 비디오 튜토리얼 및 안내
- 📊 더 상세한 성능 분석
- 🌐 다국어 문서

---

**Database System 문서** - C++20의 엔터프라이즈급 데이터베이스 추상화를 위한 포괄적인 가이드.

마지막 업데이트: 2025년 1월 19일

---

*Last Updated: 2025-10-20*
