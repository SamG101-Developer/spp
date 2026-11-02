---
name: cmake-genex-no-line-breaks
description: "A generator expression wrapped across lines splits into separate CMake arguments; ninja then fails with 'bad $-escape'"
updated: 2026-11-02
---

An unquoted CMake argument ends at whitespace, newlines included, so a generator expression broken over lines becomes
several arguments. In `add_compile_definitions` each piece gets its own `-D`, and the configure succeeds; the failure
only shows when ninja reads the result: `build.ninja:83: bad $-escape (literal $ must be written as $$)` over
`DEFINES = -D$<$<OR: -D0, -D0>:SPP_TEST_TEARDOWN>`.

**Why:** 2026-11-02, the `SPP_TEST_TEARDOWN` condition in `cmake/Runtime.cmake` was rewrapped one condition per line.

**How to apply:** keep a generator expression on one line. When it is long, build the condition in a variable first, as
`SPP_NO_MIMALLOC_GENEX` and `SPP_TEST_TEARDOWN_GENEX` do, so that no line needs wrapping. After editing one, check that
`build.ninja` has no raw `$<` left.
