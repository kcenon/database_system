# Custom triplet overlay: x64-windows with per-port dynamic linking
#
# LGPL-2.1 compliance: libmariadb must be dynamically linked to avoid
# copyleft obligations on the BSD-3-Clause project binary.
# Windows defaults to dynamic linking for most vcpkg ports, but this
# triplet ensures consistent behavior across all platforms.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Force dynamic linking for LGPL-licensed ports
if(PORT MATCHES "^libmariadb$")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
