---
name: link-flags-per-object-format
description: "_LinkExecutable picks rpath, -lm and hardening flags from the target's object format; Mach-O gets no -z, COFF links are not wired up"
updated: 2026-10-25
---

`_LinkExecutable` in `compiler_boot.cpp` branches on `codegen::TargetTriple()` (the target, not the host, so a cross
link gets the target's flags):

- **ELF:** `-lm`, `-Wl,-rpath,'$ORIGIN/lib'`, and the `[binary.link] harden` flags (`-pie`, `-z relro,now`,
  `noexecstack`, `separate-code`, `defs`, `text`).
- **Mach-O:** `-lm`, `-Wl,-rpath,@loader_path/lib`, and no hardening flags. ld64 rejects `-z` outright ("ld: unknown
  options: -z ..."), and already gives by default what those flags ask for: PIE, an NX stack, undefined symbols as
  errors, no text relocations on arm64, and chained fixups into `__DATA_CONST` in place of RELRO. `-bind_at_load` (the
  nearest to `-z now`) is ignored with a warning under chained fixups. `$ORIGIN` means nothing to dyld, so the old
  rpath would have linked and then failed to load `libsppc.dylib`.
- **COFF:** neither `-lm` (the CRT carries the maths functions) nor an rpath (PE has none). The link itself is not
  wired up yet: `LinkerCommand` returns `cc`, the output has no `.exe`, and staged DLLs sit in `lib/`, where the Windows
  loader does not look.

**Windows fails before linking anyway (2026-10-25).** On the `windows msvc` lane, `stl-conformance.sh` stops in codegen
for `x86_64-windows-msvc` with `error: no libcall available for stackprotector check fail`, once per function.
Cause: `protect` (`sspstrong`) and `split` (`safestack`) together. Either alone is fine for MSVC. LLVM's SafeStack
pass takes over the canary of any ssp function and checks it itself (`SafeStack::checkStackGuard`,
`llvm/lib/CodeGen/SafeStack.cpp`), always calling the `STACKPROTECTOR_CHECK_FAIL` libcall (`__stack_chk_fail`), which
MSVC triples do not have. It never uses the target's `__security_check_cookie` route. Reproduce locally with
`spp build -m dev --target x86_64-pc-windows-msvc`, or `llc` on any MSVC IR with both attributes. See
[[stack-protector-heuristics]].

**Why:** the ELF-only flags were added unconditionally, which broke the macOS lane's STL conformance link.

**How to apply:** any new link flag goes in the branch for its object format, with a check of what ld64 and lld-link
do by default. The post-link `UnhardenedSegments` check returns "no fault" for non-ELF files, so it never catches a
wrong Mach-O or COFF link. Related: [[spp-elf-section-hardening]].
