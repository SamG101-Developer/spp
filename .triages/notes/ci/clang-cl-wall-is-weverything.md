---
name: clang-cl-wall-is-weverything
description: "clang-cl reads -Wall as -Weverything (C++98-compat noise, ~46k warnings); Warnings.cmake gives clang-cl /W4 instead"
updated: 2026-11-07
---

clang-cl treats `-Wall` like MSVC's `/Wall`, which means `-Weverything`. With the GCC/clang warning list it printed
about 46,000 warnings per build (`-Wc++98-compat`, `-Wpre-c++17-compat`, ...). `cmake/Warnings.cmake` now passes `/W4`
(clang-cl's `-Wall -Wextra`) when `CMAKE_CXX_COMPILER_FRONTEND_VARIANT` is `MSVC`, and `-Wall -Wextra` otherwise.
The other `-W` flags are passed the same way to both drivers.

**Why:** the noise buries real errors in the clang-cl lane's log.

**How to apply:** never add `-Wall` or `-Weverything` to the shared list; put driver-specific flags under the
frontend-variant check.
