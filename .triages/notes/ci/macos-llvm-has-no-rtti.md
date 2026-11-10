---
name: macos-llvm-has-no-rtti
description: "The LLVM release tarball is built -fno-rtti; ValueMap in our TU needed typeinfo it lacks; macOS moved to brew llvm (RTTI on), floor 26"
updated: 2026-11-09
---

macOS linked the official `LLVM-<v>-macOS-ARM64.tar.xz` (`[pin.llvm-mac]`, until 2026-11-09), built with `LLVM_ENABLE_RTTI=OFF`. spp
is built with RTTI. A TU that instantiates a polymorphic LLVM class template emits its vtable and typeinfo locally,
and that typeinfo references the base class's typeinfo, which only exists in an RTTI build of LLVM. The first case
(run 42710011919, 2026-11-09) was `llvm_air.cpp`'s `llvm::ValueToValueMapTy` (for `CloneBasicBlock` and
`cloneLoopWithPreheader`): `ld64.lld: undefined symbol: typeinfo for llvm::CallbackVH`, from
`ValueMapCallbackVH<...>`'s typeinfo. Linux's apt.llvm.org libraries are built with RTTI. MSVC emits RTTI
descriptors as COMDATs in every TU, so Windows links too.

`-fno-rtti` on just that file does not work: clang's `RTTI` language option is `NotCompatible`, so a `-fno-rtti` TU
cannot import BMIs built with RTTI (`spp.utils.types`, `genex`).

The official release has only the one macOS asset (`LLVM-<v>-macOS-ARM64`, `.tar.xz`/`.tar.zst`), and both are
built without RTTI. Builds with RTTI exist: Homebrew's `llvm` formula (`-DLLVM_ENABLE_RTTI=ON`, at 23.1.5 on
2026-11-09, so the "no llvm@23" comment on `[pin.llvm-mac]` is stale) and conda-forge's `llvmdev` (23.1.5). Both
float rather than pin a tag.

**Why:** a one-platform link failure that looks like a missing library.

Fixed 2026-11-09 by moving macOS to brew's `llvm` (`install-llvm-macos.sh` fails if `llvm-config --has-rtti` is not
YES). The cost: brew builds bottles for the runner's macOS, so the static LLVM and libc++ archives raise `[floor]
macos` from 11.0 to 26.0 (`MACOSX_DEPLOYMENT_TARGET` is exported from it in `install-clang-macos.sh`). The
alternative, kept in reserve, was a plain TU with no `import`s holding the `ValueMap` code, built `-fno-rtti` when
`LLVM_ENABLE_RTTI` is OFF.

brew's LLVM also links `LLVMSupport` against `zstd::libzstd_shared` and `libz3.dylib` (Z3 solver, until LLVM 24),
which `check-self-contained.sh` rejects (run 42721403742). `cmake/Dependencies.cmake` rewrites `LLVMSupport`'s
`INTERFACE_LINK_LIBRARIES` on APPLE: static zstd in place of the dylib, and libz3 dropped, since only `Z3Solver.cpp.o`
uses it and nothing pulls that member in.

**How to apply:** if macOS goes back to a release tarball (or Windows starts linking something RTTI-dependent),
expect this again; check `llvm-config --has-rtti` first. Related: [[llvm-passes-plain-tu]], [[llvm-win-mac-pins-refreshed]].
