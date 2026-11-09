---
name: mem-ops-are-llvm-intrinsics
description: std mem_copy/mem_set are compiler intrinsics emitting llvm.memcpy/memset; sppc_memcpy/memset/memmove are gone
updated: 2026-11-09
---

`std::mem::ops::mem_copy` and `mem_set` are `!intrinsic`s (`StdMemOpsMemCopy`, `StdMemOpsMemSet` in
`../sources/spp/codegen/llvm_fn_impls.cpp`) emitting `llvm.memcpy` / `llvm.memset` at the byte offsets. They used to
call `sppc_memcpy` / `sppc_memset`, which were removed from sppc. `mem_cmp` still calls `sppc_memcmp` (LLVM has no
memcmp intrinsic; it only recognises libc's `memcmp` by name). `sppc_memmove`, which had no std user, was removed
too; an overlapping copy, if one is needed, is `llvm.memmove`, as a third intrinsic.

**Why:** an FFI call is opaque: a 1-4 byte `Str.append` stayed a call into sppc, and the optimiser could not follow
bytes through a copy (O14 follow-up, 2026-11-09).
**How to apply:** a byte-level primitive LLVM has an intrinsic for belongs in the compiler, not sppc. The two stay
excluded in `tests/codegen/intrinsics.py` (byte copies, covered by the STL tests).
