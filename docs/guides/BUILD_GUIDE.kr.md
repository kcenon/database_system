---
doc_id: "DBS-GUID-012"
doc_title: "Database System Build Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Database System Build Guide

> **SSOT**: This document is the single source of truth for **Database System Build Guide**.

> **Language:** [English](BUILD_GUIDE.md) | **한국어**

멀티 백엔드 지원, 연결 풀링 및 쿼리 빌더를 갖춘 Database System 빌드를 위한 종합 가이드입니다.

## 목차

- [사전 요구 사항](#사전-요구-사항)
- [빠른 시작](#빠른-시작)
- [빌드 구성](#빌드-구성)
- [데이터베이스 의존성](#데이터베이스-의존성)
- [플랫폼별 지침](#플랫폼별-지침)
- [문제 해결](#문제-해결)
- [고급 구성](#고급-구성)

## 사전 요구 사항

### 시스템 요구 사항

- **C++20 호환 컴파일러**:
  - GCC 10.0+ (Linux)
  - Clang 11.0+ (macOS/Linux)
  - MSVC 2019+ (Windows)
- **CMake 3.16+**
- **빌드 시스템**: Make, Ninja (권장), 또는 Visual Studio
- **Git** (vcpkg 및 클로닝용)

### 선택적 의존성

데이터베이스 지원은 선택 사항이며 테스트를 위해 비활성화할 수 있습니다:

- **PostgreSQL**: libpqxx, libpq, OpenSSL 3.0+
- **SQLite**: sqlite3
- **MongoDB**: mongo-cxx-driver (mongocxx, bsoncxx)
- **Redis**: hiredis

> Legacy MySQL/MariaDB 지원은 Issue #418에서 제거되었습니다. 현재 빌드는 `libmariadb`와 `USE_MYSQL`을 사용하지 않습니다.

## 빠른 시작

### 1. 저장소 클론

```bash
git clone https://github.com/kcenon/database_system.git
cd database_system
```

### 2. 기본 빌드 (외부 의존성 없음)

```bash
# 빌드 디렉토리 생성
mkdir build && cd build

# 모의 구현으로 구성
cmake .. -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_MONGODB=OFF -DUSE_REDIS=OFF

# 빌드
ninja  # 또는 make -j$(nproc)

# 테스트
./bin/basic_usage
./bin/connection_pool_demo
```

### 3. 데이터베이스 지원을 포함한 전체 빌드

```bash
# 의존성 설치 (데이터베이스 의존성 섹션 참조)
# 그런 다음 전체 지원으로 구성
cmake .. -DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON -DUSE_MONGODB=ON -DUSE_REDIS=ON

# 빌드
ninja
```

## 빌드 구성

### CMake 옵션

| 옵션 | 기본값 | 설명 |
|--------|---------|-------------|
| `USE_POSTGRESQL` | ON | PostgreSQL 지원 활성화 (libpqxx 필요) |
| `USE_SQLITE` | OFF | SQLite 지원 활성화 (sqlite3 필요) |
| `USE_MONGODB` | OFF | MongoDB 지원 활성화 (mongocxx 필요) - **실험적** |
| `USE_REDIS` | OFF | Redis 지원 활성화 (hiredis 필요) - **실험적** |
| `BUILD_DATABASE_SAMPLES` | ON | 샘플 프로그램 빌드 |
| `USE_UNIT_TEST` | ON | 단위 테스트 빌드 |
| `BUILD_SHARED_LIBS` | OFF | 공유 라이브러리로 빌드 |

### 실험적 백엔드

> ⚠️ **참고**: MongoDB와 Redis 백엔드는 실험적이며 기본적으로 비활성화되어 있습니다.
> 이 백엔드들은 완전히 기능하지만 향후 릴리스에서 지원이 제한되거나 Breaking Changes가 발생할 수 있습니다.

#### 실험적 백엔드 활성화

```bash
# MongoDB 활성화 (실험적)
cmake .. -DUSE_MONGODB=ON

# Redis 활성화 (실험적)
cmake .. -DUSE_REDIS=ON

# 두 실험적 백엔드 모두 활성화
cmake .. -DUSE_MONGODB=ON -DUSE_REDIS=ON
```

#### 실험적 백엔드를 위한 vcpkg Features

```bash
# MongoDB 의존성 설치
vcpkg install mongo-cxx-driver

# Redis 의존성 설치
vcpkg install hiredis

# 실험적 백엔드로 빌드
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DUSE_MONGODB=ON \
  -DUSE_REDIS=ON
```

#### 실험적 백엔드 상태

| 백엔드 | 상태 | 의존성 | 비고 |
|--------|------|--------|------|
| MongoDB | 🧪 실험적 | mongocxx, bsoncxx | NoSQL 문서 저장소, 집계 지원 |
| Redis | 🧪 실험적 | hiredis | 인메모리 데이터 저장소, Pub/Sub 지원 |

**향후 계획**: 이 백엔드들은 향후 릴리스에서 선택적 contrib 패키지로 분리될 수 있습니다. 자세한 내용은 [Issue #333](https://github.com/kcenon/database_system/issues/333)을 참조하세요.

### 빌드 타입

```bash
# Debug 빌드 (기본값)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 빌드 (최적화됨)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 디버그 정보를 포함한 Release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 최소 크기 Release
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```

### 일반적인 빌드 시나리오

#### 1. 개발 빌드

```bash
# 디버그 정보를 포함한 전체 기능
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_POSTGRESQL=ON \
  -DUSE_SQLITE=ON \
  -DBUILD_DATABASE_SAMPLES=ON \
  -DUSE_UNIT_TEST=ON
```

#### 2. 프로덕션 빌드

```bash
# 특정 데이터베이스를 사용한 최적화된 Release
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_POSTGRESQL=ON \
  -DUSE_REDIS=ON \
  -DBUILD_DATABASE_SAMPLES=OFF \
  -DUSE_UNIT_TEST=OFF
```

#### 3. 테스트/CI 빌드

```bash
# 모의 구현만 사용
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_SQLITE=OFF \
  -DUSE_MONGODB=OFF \
  -DUSE_REDIS=OFF \
  -DBUILD_DATABASE_SAMPLES=ON \
  -DUSE_UNIT_TEST=ON
```

## 데이터베이스 의존성

### vcpkg 사용 (권장)

#### vcpkg 설치

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Windows
.\bootstrap-vcpkg.bat

# Linux/macOS
./bootstrap-vcpkg.sh
```

#### 데이터베이스 라이브러리 설치

```bash
# PostgreSQL 지원
vcpkg install libpqxx openssl

# SQLite 지원
vcpkg install sqlite3

# MongoDB 지원
vcpkg install mongo-cxx-driver

# Redis 지원
vcpkg install hiredis

# 한 번에 모두 설치
vcpkg install libpqxx openssl sqlite3 mongo-cxx-driver hiredis
```

#### vcpkg로 빌드

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DUSE_POSTGRESQL=ON \
  -DUSE_SQLITE=ON \
  -DUSE_MONGODB=ON \
  -DUSE_REDIS=ON
```

### 수동 설치

#### Ubuntu/Debian

```bash
# PostgreSQL
sudo apt-get install libpqxx-dev libpq-dev libssl-dev

# SQLite
sudo apt-get install libsqlite3-dev

# MongoDB
sudo apt-get install libmongocxx-dev libbsoncxx-dev

# Redis
sudo apt-get install libhiredis-dev
```

#### CentOS/RHEL/Fedora

```bash
# PostgreSQL
sudo dnf install libpqxx-devel postgresql-devel openssl-devel

# SQLite
sudo dnf install sqlite-devel

# MongoDB
sudo dnf install mongo-cxx-driver-devel

# Redis
sudo dnf install hiredis-devel
```

#### macOS (Homebrew)

```bash
# PostgreSQL
brew install libpqxx postgresql openssl

# SQLite
brew install sqlite

# MongoDB
brew install mongo-cxx-driver

# Redis
brew install hiredis
```

#### Windows (vcpkg 권장)

Windows의 경우 vcpkg가 권장되는 방법입니다. 의존성 관리로 인해 수동 설치는 복잡합니다.

## 플랫폼별 지침

### Linux

```bash
# 의존성 설치
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git

# 데이터베이스 라이브러리 설치 (위 참조)

# 빌드
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja

# 설치 (선택 사항)
sudo ninja install
```

### macOS

```bash
# Xcode 명령줄 도구 설치
xcode-select --install

# 의존성 설치
brew install cmake ninja

# 데이터베이스 라이브러리 설치 (위 참조)

# 빌드
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
```

### Windows

#### Visual Studio 사용

```batch
# 개발자 명령 프롬프트 열기

# 빌드
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

#### MSYS2/MinGW 사용

```bash
# 먼저 MSYS2 설치

# 의존성 설치
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

# 빌드
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
```

## 문제 해결

### 일반적인 빌드 문제

#### 1. C++20 지원 누락

**오류**: `error: 'std::variant' is not available before C++17`

**해결책**:
```bash
# 컴파일러 업데이트
# GCC
sudo apt-get install gcc-10 g++-10
export CC=gcc-10 CXX=g++-10

# Clang
sudo apt-get install clang-11
export CC=clang-11 CXX=clang++-11
```

#### 2. 데이터베이스 라이브러리 누락

**오류**: `Could NOT find libpqxx (missing: libpqxx_LIBRARY libpqxx_INCLUDE_DIR)`

**해결책**:
```bash
# 필요하지 않은 경우 특정 데이터베이스 비활성화
cmake .. -DUSE_POSTGRESQL=OFF

# 또는 라이브러리 설치
sudo apt-get install libpqxx-dev

# 또는 vcpkg 사용
vcpkg install libpqxx
```

#### 3. CMake 버전이 너무 오래됨

**오류**: `CMake 3.16 or higher is required. You are running version 3.10.2`

**해결책**:
```bash
# Ubuntu/Debian
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt-get update
sudo apt-get install cmake

# 또는 소스에서 빌드
wget https://github.com/Kitware/CMake/releases/download/v3.26.0/cmake-3.26.0.tar.gz
tar -xzf cmake-3.26.0.tar.gz
cd cmake-3.26.0
./bootstrap && make -j$(nproc) && sudo make install
```

#### 4. 링킹 오류

**오류**: `undefined reference to 'pqxx::connection::connection(...)'`

**해결책**:
```bash
# 모든 의존성이 발견되었는지 확인
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON

# 라이브러리가 제대로 링크되었는지 확인
ldd bin/basic_usage

# 정적 링킹 문제의 경우
cmake .. -DBUILD_SHARED_LIBS=OFF
```

#### 5. MongoDB 드라이버 문제

**오류**: `Could NOT find mongocxx`

**해결책**:
```bash
# MongoDB C++ 드라이버를 수동으로 설치
git clone https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc) && sudo make install
```

### 빌드 검증

성공적인 빌드 후 설치 확인:

```bash
# 빌드된 파일 확인
ls -la bin/
ls -la lib/

# 테스트 실행
ctest --verbose

# 샘플 실행
./bin/basic_usage
./bin/connection_pool_demo
./bin/postgres_advanced  # PostgreSQL이 활성화된 경우

# 의존성 확인
ldd bin/basic_usage  # Linux
otool -L bin/basic_usage  # macOS
```

## 고급 구성

### 사용자 정의 빌드 옵션

#### 특정 기능 비활성화

```bash
# 최소 빌드 - 핵심 기능만
cmake .. \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_SQLITE=OFF \
  -DUSE_MONGODB=OFF \
  -DUSE_REDIS=OFF \
  -DBUILD_DATABASE_SAMPLES=OFF \
  -DUSE_UNIT_TEST=OFF
```

#### 사용자 정의 설치 디렉토리

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/database_system
ninja install
```

#### 크로스 컴파일

```bash
# ARM64 크로스 컴파일 예제
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=arm64-toolchain.cmake \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_SQLITE=OFF
```

### 환경 변수

```bash
# 사용자 정의 컴파일러
export CC=/usr/bin/clang-12
export CXX=/usr/bin/clang++-12

# 사용자 정의 라이브러리 경로
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# vcpkg 통합
export VCPKG_ROOT=/path/to/vcpkg
export CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

### 성능 최적화

#### Release 빌드

```bash
# 최대 최적화
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG"

# 링크 타임 최적화
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

#### 메모리 최적화

```bash
# 최소 크기 빌드
cmake .. \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_CXX_FLAGS="-Os -flto"
```

### IDE 통합

#### VS Code

```json
// .vscode/settings.json
{
    "cmake.configureSettings": {
        "USE_POSTGRESQL": "ON",
        "USE_SQLITE": "ON",
        "CMAKE_BUILD_TYPE": "Debug"
    },
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

#### CLion

Settings → Build, Execution, Deployment → CMake에서 CMake 옵션 구성:

```
-DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON
```

#### Visual Studio

vcpkg와 함께 CMake 통합 사용:

```json
// CMakeSettings.json
{
  "configurations": [
    {
      "name": "x64-Release",
      "generator": "Ninja",
      "configurationType": "Release",
      "inheritEnvironments": [ "msvc_x64_x64" ],
      "buildRoot": "${projectDir}\\build\\${name}",
      "installRoot": "${projectDir}\\install\\${name}",
      "cmakeCommandArgs": "-DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON",
      "ctestCommandArgs": "",
      "variables": [
        {
          "name": "CMAKE_TOOLCHAIN_FILE",
          "value": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        }
      ]
    }
  ]
}
```

## 지속적 통합

### GitHub Actions 예제

```yaml
# .github/workflows/build.yml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest

    strategy:
      matrix:
        compiler: [gcc-10, clang-11]
        build_type: [Debug, Release]

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install ${{ matrix.compiler }} cmake ninja-build

    - name: Configure
      run: |
        mkdir build && cd build
        export CC=${{ matrix.compiler }}
        export CXX=${CC/gcc/g++}
        export CXX=${CXX/clang/clang++}
        cmake .. -GNinja -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

    - name: Build
      run: |
        cd build
        ninja

    - name: Test
      run: |
        cd build
        ctest --verbose
```

### Docker 빌드

```dockerfile
# Dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    libpqxx-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
    -DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON && \
    ninja

CMD ["./build/bin/basic_usage"]
```

---

여기에서 다루지 않은 추가 도움말이나 문제는 다음을 참조하세요:
1. [문제 해결 가이드](TROUBLESHOOTING.md) 확인
2. 기존 [GitHub issues](https://github.com/kcenon/database_system/issues) 검색
3. 빌드 구성 및 오류 세부 정보와 함께 새 이슈 생성

---

*Last Updated: 2025-10-20*
