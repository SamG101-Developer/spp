---
name: windows-h-before-sdk-headers
description: "Windows SDK headers like psapi.h don't include windows.h; sorted alphabetically they break the MSVC build of sys.ixx"
updated: 2026-11-06
---

`psapi.h` (and other Win32 SDK headers) use `BOOL`, `DWORD` and `WINAPI` without including `windows.h`. An
alphabetical include list puts `psapi.h` first, and MSVC then fails in `headers/wrappers/sys.ixx` with a wall of
C2146 "missing ';' before identifier 'WINAPI'" and C2086 "'int BOOL': redefinition" errors. Only the Windows lane
sees it.

**Why:** de040f05 added `psapi.h` in sorted order above `windows.h` and broke the Windows build.

**How to apply:** keep `windows.h` first, in its own block, in any GMF that includes a Win32 SDK header. A GCC or clang
build on Linux can't catch this.
