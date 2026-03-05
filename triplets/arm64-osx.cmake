# Custom triplet overlay: arm64-osx with per-port dynamic linking
#
# LGPL-2.1 compliance: libmariadb must be dynamically linked to avoid
# copyleft obligations on the BSD-3-Clause project binary.
# All other ports use static linking (default).

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Force dynamic linking for LGPL-licensed ports
if(PORT MATCHES "^libmariadb$")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
