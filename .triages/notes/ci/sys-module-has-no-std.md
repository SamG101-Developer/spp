---
name: sys-module-has-no-std
description: headers/wrappers/sys.ixx imports no std; a braced-list range-for there builds on Linux and fails on every Windows lane
updated: 2026-11-12
---

`headers/wrappers/sys.ixx` (module `sys`) wraps the C headers and imports no std. Anything that needs a std type
compiles on Linux only because some C header there happens to declare it. `for (auto x : {a, b})` needs
`std::initializer_list`: GCC and clang on Linux built it, and all three Windows lanes failed (MSVC `C3312: no
callable 'begin' function found for type 'initializer list'`; clang-cl "std::initializer_list was not found"), in
`use_utf8_console` (2026-11-12). Fixed with a local lambda called once per handle.

**Why:** the Windows branches of `sys` (`#if SPP_PLATFORM_WINDOWS`) are compiled by no local build, so the first sign
is CI.

**How to apply:** in `sys.ixx`, use only what the C headers give: no braced-list loops, no std types or algorithms. Do
not fix it with `#include <initializer_list>` (see [[import-std-not-stl-headers]]) or `import std;` (see
[[sys-names-vs-c-macros]]).
