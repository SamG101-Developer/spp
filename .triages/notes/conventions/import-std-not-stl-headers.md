---
name: import-std-not-stl-headers
description: "In spp, reach for `import std` rather than including STL headers in a module's global module fragment"
updated: 2026-08-18
---

Use `import std;` for standard-library facilities in spp; do not add `#include <vector>`, `<unistd.h>`-style
STL/system headers to a TU's global module fragment to get them.

**Why:** the project is built as C++26 modules with `CMAKE_CXX_MODULE_STD ON`; mixing header includes with the
std module duplicates declarations and undermines the module build.

**How to apply:** if something is only reachable through a C/POSIX header (e.g. `isatty` from `<unistd.h>`,
`_isatty` from `<io.h>`), say so and offer the header-free alternative rather than silently adding the include.
The exception is a genuinely non-std third-party header that has to sit in the GMF — see
[[boost-multiprecision-needs-gmf]].
