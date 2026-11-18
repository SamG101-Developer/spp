---
name: thread-local-guards-scoped
description: "A thread_local or static guard set or value is only touched through utils::functions::ScopedInsert/ScopedAssign: a hand erase skipped by a throw leaks into the next compile"
updated: 2026-11-18
---

Recursion guards and save-and-restore state that outlive one call (a `thread_local` or `static` set, depth or slot)
go through `spp::utils::functions`: `ScopedInsert(set, key)` (inserts, erases on destruction only if it inserted;
`Inserted()` says whether it was absent) and `ScopedAssign(var, value)` (sets, restores the old value on destruction;
`ScopedAssign(depth, depth + 1)` for a counter). Both undo on a throw too. A named guard other modules use
(`CompTimeResolutionGuard`, `InstantiationDepth`, `PackElementCloneGuard`) keeps its name and holds one as a member.

**Why:** errors are exceptions, and `spp_tests` runs many compiles in one process. `EnsureLlvmTypeComplete` inserted
the struct it was laying out into a `thread_local` set, called `FillLlvmLayout`, then erased it by hand; an `!ffi`
attribute error thrown from the layout skipped the erase. The stale pointer stayed, a later compile's struct at the
same address was taken for one in progress, its layout (and `!ffi` check) skipped: `FfiConventions.
test_invalid_generic_ffi_class_instantiated_with_a_class_laid_out_the_spp_way` failed about 1 run in 4 (2 of 8;
0 of 24 after). `ConstantCompKey` had the same shape. Found 2026-11-18.

**How to apply:** never insert-then-erase or set-then-restore by hand around a call that can raise. Inside
`spp::analyse::...` spell it `spp::utils::functions::`, as `utils::` finds `spp::analyse::utils` first. A flaky
"nothing was thrown" in a batch that passes alone is this class of bug first.
