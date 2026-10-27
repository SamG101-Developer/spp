---
name: use-normalised-platform-macros
description: "Guard compiler/OS-specific code with SPP_COMPILER_* / platform macros from spp/macros-platforms.hpp, not raw _MSC_VER etc."
updated: 2026-10-13
---

Compiler and OS guards use the normalised macros in `headers/spp/macros-platforms.hpp` (`#if SPP_COMPILER_MSVC`, `SPP_COMPILER_GCC`, `SPP_COMPILER_CLANG`, plus the OS ones). They are always defined as 0 or 1, so test them with `#if`, not `#ifdef`. Never test raw `_MSC_VER` / `__GNUC__` / `__clang__` directly.

**Why:** the maintainer corrected a raw `#ifdef _MSC_VER` in headers/wrappers/gch.ixx (2026-10-13). One place does the detection; clang-cl also defines `_MSC_VER`, but `SPP_COMPILER_MSVC` stays 0 for it.

**How to apply:** In a GMF or wrapper that has no macros, `#include <spp/macros-platforms.hpp>` first. See [[third-party-module-wrappers]].
