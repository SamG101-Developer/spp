---
name: spp-single-abort-path
description: "Runtime checks fail via sppc_check_failed (_exit 134); never emit willreturn from the frontend"
updated: 2026-10-17
---

S15.2 (2026-10-17): every compiler-emitted runtime check calls `sppc_check_failed(u32 check, u64 a, u64 b)` via
`EmitCheckFailed` in llvm_fn_impls.cpp. `RuntimeCheck` numbers mirror sppc's `SPPC_CHECK_*` enum in
`headers/sppc/platform/posix.h`, so messages live in sppc and numbers are append-only. It reports + backtraces through
the crash reporter's buffer (`_sppc_bt_fail_begin/_end` in backtrace/elf.h) then `_exit(134)`; `sppc_abort` takes the
same path. Overflow joined it in S3.8 (by name only, so failure blocks merge per kind per function; cost: a
non-inlined leaf gains push/pop, shrink-wrap won't sink alignment onto an `unreachable` edge).

A new check needs a new libsppc.so in SPP-STL: tell the maintainer sppc changed; their push makes CI open the SPP-STL PR (see
[[sppc-sibling-repo]]); before then test with
`SPPC_LIB=../sppc/<release build dir>/libsppc.so tests/codegen/check-runtime-checks.sh`.

Float-to-int (S5.6, check 6) checks in `SimpleIntrinsicConv` via `EmitFloatToIntCheck`: abort, not saturate - the maintainer's
call, same reasoning as checked `+`.

Check 7 (S20.1): `!ffi` decls carry `spp-ffi` (not `nounwind`); `ApplyFfiUnwindGuard` (llvm_harden_codegen.cpp) makes each
call an invoke whose cleanup pad calls `sppc_check_failed(7, ptr-to-symbol-name)`, personality `__gcc_personality_v0`.

**Never add `willreturn` from the frontend.** It was on every non-Never fn; under rel LLVM then treated a failed check's
noreturn call as UB and deleted the bounds-check failure path (OOB index ran off `main`, SIGSEGV at 0). LLVM infers it.

**Why:** a deleted failure path silently turns a checked operation into an unchecked one.
**How to apply:** any new runtime check uses `EmitCheckFailed` + a new kind on both sides; verify it at `rel`, not just
`dev`. Related: [[checked-arithmetic-is-default]], [[spp-elf-section-hardening]], [[sppc-sibling-repo]].
