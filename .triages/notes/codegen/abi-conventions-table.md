---
name: abi-conventions-table
description: "!conv names a codegen::Abi (spp, C) for class layout + function calls; !ffi implies C; called Abi as Convention means &/&mut"
updated: 2026-11-10
---

`!conv("name")` on a class or function picks an entry of the convention table in `spp.codegen.llvm_abi`
(`FindAbi`): today `SppAbiImpl` ("spp", the default) and `CAbiImpl` ("C"). Each entry implements every hook in one
place: `LayOutStruct` (field order), `ParamsOf`/`ReturnOf`/`AddAttrs` (declaration), `AdaptArgs`/`AdaptReturn` (call
site). `!ffi` on a function or class implies "C" unless a `!conv` names another, and public (set in stage 4, with
`!public`, so stubs write no `!public`); tuples are always "C". Read a
convention through `codegen::AbiOf(symbol)` / `AbiOf(prototype)`, never `DeclaredAbi` directly (null = default).

Rules, raised as `SppIdentifierUnknownError` until conventions are enums: an unknown name; a foreign convention
(`IsForeign`) on a function defined in s++; a non-foreign one on an `!ffi` function or class. An `!ffi` class's
attributes must be ones C can hold, checked per instance in `FillLlvmLayout` (`SppFfiAttributeTypeError`, E127);
`!conv("C")` alone fixes the order and checks nothing. An `!ffi` function with no `symbol` binds to its own name.

**Why "Abi":** `TypeSymbol::Convention`, `ConventionAst` and `WithoutConvention` already mean a borrow's convention
(`&`, `&mut`), so the table is `Abi` in the compiler and `!conv` only in source.
**Not entries yet:** "packed" (a borrow of a packed field would still get its type's `align`); definitions C calls
(callbacks, exports) and returned C structs ("sret"); a convention on function pointer types.
Related: [[ffi-struct-params-c-abi]], [[spp-cls-layout-reorders-fields]], [[merge-spp-before-stl-for-new-features]].
