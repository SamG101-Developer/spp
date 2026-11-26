---
name: llvm-optimisation-hints
description: Which LLVM hints codegen emits (inbounds index GEPs, tag !range) and which are left out on purpose
updated: 2026-11-26
---

Index GEPs are `inbounds` (view index/iter/slice after their bounds checks, `raw_buf` by its contract, array
literals, constant array member access, drop and case paths). The exception is `fn_impls::MemOpsAddress`, which stays
plain because a prefetch may point past the end of what it is given. Variant tag loads (`LoadVariantTag`) carry
`!range [0, member_count)`; every caller passes the member count from `VariantMemberRefs`.

Left out on purpose: lifetime markers (allocas all sit in the entry block with none, except the coroutine
start-only markers in `coroutine_prototype_ast.cpp`), `mustprogress` (S++ allows an infinite `loop`), `willreturn`
(checked ops abort), and TBAA (borrow rules plus `noalias` cover most of it). `captures(none)` on borrows is still a
Todo in `function_prototype_ast.cpp`.

Measured 2026-11-26 on 20 runtime benchmarks: no change in instruction counts except fannkuch (-0.8%). In optimised
IR, LLVM had already inferred `inbounds` for nearly every GEP in the hot loops, and the `!range` is gone because SROA
turns stack variants into SSA values whose stored tags it can see. The hints only matter where nothing is inlined.

**Why:** An `inbounds` GEP outside its object, or a tag load outside its range, is poison. That's a silent
miscompile, not a crash.

**How to apply:** A new index GEP is `CreateInBoundsGEP` only if a check or contract keeps it inside the object (one
past the end is allowed). A new tag load goes through `LoadVariantTag`. A variant written without `StoreVariant` or a
zeroed slot (FFI, raw memory) breaks the `!range` assumption.
