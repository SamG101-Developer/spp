---
name: sppc-sibling-repo
description: "the sppc C shim is a sibling repo at ../sppc whose .so is vendored into SPP-STL; a C-side fix is often cleaner than an s++ workaround"
updated: 2026-09-11
---

`sppc` - the C shim behind every `sppc::` call in the standard library - lives at `../sppc`, a sibling of the
`spp` root, and is fair game to change. Its build output
(`<release build dir>/libsppc.so`) is vendored as `libsppc.so` in **two** places, both needing the copy:
`project/vcs/std/ffi/sppc/libsppc.so` (the standard library) and `project/ffi/sppc/libsppc.so` (the project's own).
The old `ffi/sppc/lib/sppc.so` layout is gone (verified 2026-09-11). So a change there means: build sppc, copy the
`.so` over both paths, then rebuild the project. The exported set is `sppc_*` via `sppc.map`, so compare
`nm -D --defined-only` counts before and after a rebuild.

sppc also owns the crash handler (`headers/sppc/backtrace.h`, installed by `sppc_init`), which needs the compiler's
`ApplyUnwindTables` to see past the first s++ frame; future work is in `.triages/docs_open/debug-info-and-backtraces.md`. Since
2026-09-22 it buffers the report and writes it once (per-fragment writes interleaved with other threads' println), and
no longer uses `SA_RESETHAND`: a second thread faulting mid-report now parks in the handler instead of killing the
process before the report is flushed.

The library is header-implemented (`headers/sppc/sppc.h`, with `sources/sppc/sppc.c` redeclaring the entry points to
force emission) and macro-heavy (`headers/sppc/macros.h`).

**Shipping (2026-10-17):** pushing sppc auto-generates an SPP-STL PR with the new binary; once merged, `s++ build/run`
auto-pulls it. So after editing sppc, just tell the maintainer it changed - don't copy the .so into ../spp-stl yourself. Local
testing before then still means copying the .so into the project/scratch fixture (or `SPPC_LIB` for
`tests/codegen/check-runtime-checks.sh`).

**Why:** an FFI-shaped bug is usually far cheaper to fix on the C side. The `pthread_mutex_t` handle bug was a
six-module change plus a new helper module in s++, and two macros plus five functions in sppc - and the C fix left the
standard library completely untouched, because the s++ code was already written against the opaque-handle contract the
C side had not implemented.

**How to apply:** before working around a C-side limitation in s++, look at whether sppc can just provide what the s++
code already assumes. Check `git -C ../sppc status` first - unrelated in-progress work there ends up in any
`.so` you build.
