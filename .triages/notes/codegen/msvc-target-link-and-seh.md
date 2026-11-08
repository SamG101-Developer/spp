---
name: msvc-target-link-and-seh
description: "An *-windows-msvc target links with lld-link (import .lib, DLL beside the .exe, static CRT) and guards FFI calls with SEH cleanuppads, not __gcc_personality_v0"
updated: 2026-11-08
---

An MSVC target (`isWindowsMSVCEnvironment`) is not linked or unwound the GNU way:

- **Linker:** `lld-link` by default (`LinkerCommand`, `compiler_boot.cpp`), not `cc`: on a Windows runner `cc` is
  MinGW's gcc, whose runtime lacks the MSVC one's `__security_cookie` / `__C_specific_handler`. `[target.linkers]`
  still overrides it (`link` from a developer prompt). `utils::files::FindProgram` tries `name.exe` on Windows.
- **Link:** `LinkMsvcExecutable` (anonymous namespace, `compiler_boot.cpp`): `/out:<name>.exe`, the object, each ffi
  library's import library (`ffi::ImportLibraryOf`: `sppc.lib` beside `sppc.dll`), then `libcmt libvcruntime libucrt
  kernel32` (the static C runtime: the object names no runtime itself, unlike clang-compiled C). DLLs are copied beside
  the `.exe`, not into `lib/`: Windows has no rpath. `ffi::Validate` requires the `.lib` beside the `.dll` for such a
  target folder.
- **Executable name:** `OutLayout::ExecutableName` adds `codegen::ExecutableSuffix` (`.exe` on Windows).
- **FFI unwind guard:** `ApplyFfiUnwindGuard` uses `__C_specific_handler` (`_except_handler3` on 32-bit x86) and a
  `cleanuppad` whose `sppc_check_failed` call carries the `funclet` bundle; the guard runs after the module is
  verified, so a malformed funclet would only show in codegen.

**Why:** the first Windows STL conformance run (2026-11-08) linked the MSVC object with MinGW's ld and failed on
`__security_cookie`, `__security_check_cookie` and `__gcc_personality_v0`.

**How to apply:** check an MSVC-target change without Windows by cross-building a scratch project with
`-t x86_64-pc-windows-msvc` (set `split = false` under `[memory.stack]`) and reading `llvm-readobj-23 --unwind
spp.o`: guarded functions get `?dtor$N@...` funclets and a `__C_specific_handler` handler. The link itself only runs
on Windows. lld-link finding the MSVC/SDK libraries without a developer prompt is assumed, not yet seen on the runner.
