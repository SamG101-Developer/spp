---
name: sls-thunks-macho
description: "AArch64 SLS indirect-call thunks fail to link on Mach-O (LLVM calls them by the unprefixed name), so sls is refused on arm64 macOS"
updated: 2026-10-26
---

On AArch64 Mach-O, LLVM's SLS hardening defines each indirect-call thunk as `___llvm_slsblr_thunk_xN` but calls
`__llvm_slsblr_thunk_xN` (no Mach-O `_` prefix), so every macOS arm64 link fails with undefined
`__llvm_slsblr_thunk_x*` symbols. `+harden-sls-nocomdat` does not help: it only changes the thunk's linkage. ELF
targets are fine. Upstream-watch U16 has the cause, reproduction and the change to make once fixed.

`TargetHasSlsHardening` (`llvm_harden_codegen.cpp`) is false for AArch64 Mach-O, so `[binary.codegen] sls` is refused
there with the "set 'sls = false'" message, and the macOS CI test calls pass `off: sls`.

**Why:** found 2026-10-26 on the macOS STL conformance link, once the Mach-O link flags were fixed
([[link-flags-per-object-format]]). Keeping only the working `retbr` half under the same key would harden less
without saying so ([[hardening-needs-feature-gate]]).

**How to apply:** an undefined `__llvm_*_thunk_*` symbol on a Mach-O link is this bug class. A cross-built
`aarch64-apple-macos` object (all-targets build, `spp build --target`) plus `llvm-nm -u` lists every undefined
symbol without a Mac.
