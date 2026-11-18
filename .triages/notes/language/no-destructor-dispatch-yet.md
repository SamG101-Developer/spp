---
name: no-destructor-dispatch-yet
description: "implicit destructors are GONE - linear typing replaced them; EmitDrop survives for the drop intrinsics, closure captures and coroutine frames"
updated: 2026-11-15
---

**Superseded as of 2026-08-26.** The implicit-destructor machinery this file used to describe has been deleted as
part of the linear-types work (see [[spp-linear-types]]). Do not go looking for it.

Removed outright: `EmitScopeDrops`, `EmitUnwindDrops`, `EmitDiscardedValueDrop`, `NeedsDropFlag`,
`GetOrCreateDropFlag`, `EmitDropFlagSet`/`Clear`, `LlvmVarSymbolInfo::DropFlag`, `MemoryInfo::LlvmAstMoveSites`,
`Scope::BodyMemoryAnalysed`. `sources/spp/codegen/llvm_drop.cpp` is now ~110 lines holding `EmitDrop` alone.

What survives and why: `EmitDrop` (custom destructor first, then attributes in reverse declaration order) plus
`drop_utils::NeedsDrop` / `FindDropOverload`. Nothing calls them at a scope exit any more - they exist for the
intrinsics that destroy a value (`StdMemOpsDrop`, and `StdRawBufClearRange`, how a container destroys elements it
owns but has no name for), a closure dropped uncalled (its by-value captures), and a coroutine frame destroyed at a
suspension or before it was ever resumed (`EmitSuspendDestroy`). Each must mint the destructor itself, see
[[drop-needs-forced-instantiation]]. `drop_in_place` is gone from both repos (compiler side removed 2026-11-08): owning types
move the value out with `NonNull.read` and `drop` it (SPP-STL note `destroy-heap-values-by-read`).

`Del` is renamed to `Drop` throughout, and **`drop` takes `self` by move, not `&mut self`** - that is what makes
calling it discharge the linear obligation. Consequence in codegen: `EmitDrop` loads the value out of the storage and
passes it by value, because a `mov self` parameter lowers to the struct type, not to a pointer
(`function_prototype_ast.cpp` `_IsPureGn`). `common_types::DelType`/`DEL` are now `DropType`/`DROP`, naming
`std::ops::drop::Drop`.

Still true and still worth knowing: **Stage8 runs on generic instantiations**, via
`FunctionPrototypeAst::AnalysePendingGnSubstitutions`, which walks into the instantiation's scope and calls
`_CheckBodyMemory` rather than `Stage8_CheckMemory` (whose `SPP_ASSERT(sm->CurrentScope == _Scope)` fails because an
instantiation's prototype still names the template's scope). `_CheckBodyMemory` is shared with the template's own
path, so a step added there (the coroutine's `Stage8_AfterParams`, which was missed here once) reaches both.
