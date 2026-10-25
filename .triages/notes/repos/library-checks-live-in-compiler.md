---
name: library-checks-live-in-compiler
description: "Hardening checks on C/ffi libraries go in the compiler's link (ffi::Check*), never in each library's own build"
updated: 2026-10-21
---

Checks on what a library was built with (RELRO, fortify, segment layout, ...) are made by the compiler at link time,
over every ffi library at once (`ffi::CheckFortified`/`CheckRelro`/`CheckSegments` in `compiler_boot.cpp`), each
behind its own `[binary.link] *_ffi` key, default on. Not as a post-build script in sppc or any other library's repo.

**Why:** the maintainer, 2026-10-21, dropping a sppc `check-segments.sh` POST_BUILD: "all these checks need to be done from the
compiler, as its the single source where all the libs will be inspectable at the same time". sppc is only the first C
library; other spp libs will ffi their own.

**How to apply:** a library repo (sppc) may change its link *flags* so it builds, but the guarantee is a compiler
check over `ffi::Libraries`, plus a `tests/codegen/check-*-ffi.sh` end-to-end test. Related:
[[hardening-needs-feature-gate]], [[sppc-sibling-repo]].
