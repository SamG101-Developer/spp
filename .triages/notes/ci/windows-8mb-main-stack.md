---
name: windows-8mb-main-stack
description: "spp and spp_tests link /STACK:8388608 on Windows (an INTERFACE option on spp_lib): the 1 MB default overflowed at 1000 nested comp-time calls on ARM64"
updated: 2026-11-18
---

Windows gives a program's main thread a 1 MB stack, Linux and macOS 8 MB. The compiler recurses deeply: each nested
compile-time call (`kMaxCompTimeCallDepth`, 1000, in `postfix_expression_operator_function_call_ast.cpp`) is many C++
frames (the body, its statements and expressions, the call), and on Windows ARM64, whose MSVC frames are larger,
`TestCompTimeValues.test_invalid_unbounded_recursion` died with `STATUS_STACK_OVERFLOW` (exit code 3221225725,
0xC00000FD) before the limit raised its error (2026-11-18; x64 still fitted). `CMakeLists.txt` now gives
`spp_lib` the INTERFACE link option `/STACK:8388608`, so `spp.exe` and `spp_tests.exe` get 8 MB as on the other
platforms; `link.exe` and `lld-link` both take it.

**Why:** lowering the depth limit would only hide it; long expression chains and deep nesting recurse the same way.

**How to apply:** a Windows-only crash with exit code 3221225725 is a stack overflow. A new executable built on
`spp_lib` inherits the option; one that is not needs its own.
