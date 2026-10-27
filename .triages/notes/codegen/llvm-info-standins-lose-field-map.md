---
name: llvm-info-standins-lose-field-map
description: "LlvmType and FieldIndexMap must travel together; a stand-in symbol adopting only the type reads the wrong struct field"
updated: 2026-09-05
---

The `Spp` struct layout **re-orders fields by alignment**, so a declaration index is never a GEP index — everything
must go through `GetPhysicalFieldIndex(sym->LlvmInfo, decl_index)`, which reads `LlvmTypeSymbolInfo::FieldIndexMap`. An
**empty map means identity**, so a lost map fails silently as "declaration order", not as a crash at the mapping site.

`LlvmTypeSymbolInfo` is `{LlvmType, FieldIndexMap}` and is *shared* by `TypeSymbol`'s copy constructor — so copied
symbols are fine. The danger is a **freshly minted stand-in**: `Self`, or a generic parameter bound to a concrete type
while monomorphising. It owns a new `LlvmInfo` and adopts the lowered type in `EnsureLlvmTypeComplete`
(`llvm_type.cpp`). Both adoption sites used to copy `LlvmType` only, leaving the map empty while the struct pointer
looked perfectly correct. Fixed 2026-09-05 with `AdoptLlvmTypeInfo`, which copies both — **keep them together at any
new adoption site.**

**The bug it caused:** `drop(p)` on a `Pair[K=S32, V=Vec[S32]]` segfaulted. The layout is `{Vec, i32}` (`val` first),
but `std::mem::ops::drop` resolves `T` — a stand-in — and `EmitDrop`'s field-by-field loop GEP'd declaration index 1,
loading the `i32` key and calling `Vec::drop` on it. Member access on the *same* type worked, because it resolves the
real class symbol whose map is populated; that asymmetry is the tell.

**How to apply:** the field-by-field drop path in `llvm_drop.cpp` only runs for a type with **no `Drop` of its own**
(`FindDropOverload` returns null first), which is rare — there was exactly *one* such drop in the whole std test
program, so this path is nearly untested and worth suspecting. To check any GEP bug of this shape, dump
`out/<target>/<mode>/llvm/**/*.ll`, find the `= type { ... }` line for the struct and compare it against the emitted
`getelementptr ... i32 0, i32 N` — see [[spp-ir-diff-verification]]. Related: [[drop-needs-forced-instantiation]],
[[scope-symbol-family-duplication]], [[no-destructor-dispatch-yet]].
