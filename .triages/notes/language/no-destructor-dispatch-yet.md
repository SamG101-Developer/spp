---
name: no-destructor-dispatch-yet
description: "implicit destructors are GONE - linear typing replaced them; only EmitDrop survives, for drop_in_place"
updated: 2026-08-26
---

**Superseded as of 2026-08-26.** The implicit-destructor machinery this file used to describe has been deleted as
part of the linear-types work (see [[spp-linear-types]]). Do not go looking for it.

Removed outright: `EmitScopeDrops`, `EmitUnwindDrops`, `EmitDiscardedValueDrop`, `NeedsDropFlag`,
`GetOrCreateDropFlag`, `EmitDropFlagSet`/`Clear`, `LlvmVarSymbolInfo::DropFlag`, `MemoryInfo::LlvmAstMoveSites`,
`Scope::BodyMemoryAnalysed`. `sources/spp/codegen/llvm_drop.cpp` is now ~110 lines holding `EmitDrop` alone.

What survives and why: `EmitDrop` (custom destructor first, then attributes in reverse declaration order) plus
`drop_utils::NeedsDrop` / `FindDropOverload`. Nothing calls them at a scope exit any more - they exist only for the
intrinsics that destroy a value through a pointer, `StdMemOpsDropInPlace` and `StdRawBufClearRange`, which is
how a container destroys elements it owns but has no name for.

`Del` is renamed to `Drop` throughout, and **`drop` takes `self` by move, not `&mut self`** - that is what makes
calling it discharge the linear obligation. Consequence in codegen: `EmitDrop` loads the value out of the storage and
passes it by value, because a `mov self` parameter lowers to the struct type, not to a pointer
(`function_prototype_ast.cpp` `_IsPureGn`). `common_types::DelType`/`DEL` are now `DropType`/`DROP`, naming
`std::ops::drop::Drop`.

Still true and still worth knowing: **Stage8 runs on generic instantiations**, via
`FunctionPrototypeAst::AnalysePendingGnSubstitutions`, which walks into the instantiation's scope and runs
`FnParamGroup`/`Impl` Stage8 directly rather than calling `Stage8_CheckMemory` (whose
`SPP_ASSERT(sm->CurrentScope == _Scope)` fails because an instantiation's prototype still names the template's scope).
The linear scope-exit check is wired into that path too. Coroutine frames remain the known gap.
