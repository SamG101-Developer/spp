---
name: codegen-coercion-sites
description: "Every slot of a declared type goes through codegen::CoerceToSlot (spp.codegen.llvm_coerce, N5.3 2026-10-24); the site list, its analysis twin InstantiateFnValue's holes, and how to test a slot (gtest can't see wrong values)"
updated: 2026-10-23
---

**One entry point (N5.3, 2026-10-24):** `codegen::CoerceToSlot(val, target, source, sm, name, ctx)` in
`spp.codegen.llvm_coerce` is the only exported coercion; fn-value, variant and structural halves are private to it.
It does `!` → poison, tuple/array element-wise (recurses when llvm types differ OR `Id`s differ: a `$f` mock and a
`FunRef` both lower to `{ptr, ptr}`), then fn value, then variant.

**Sites:** `ret`, `let`, assignment, call args, closure-call result, object-init attrs, `gen`, `.res()` yield and send
value, case branch (whenever the case sets `AssignmentTargetType` - it does only when it yields a value), closure body,
loop `exit`/`else` (type on `LlvmLoopInfo::Type`), explicit array literal elements (to the literal's `T`). Repeated
array literal and tuple literals are covered by the structural recursion at the outer slot. `cmp` (2026-10-24, #38):
in a constant context a member→variant is a packed struct laid out as the variant (tuples/arrays holding one too); the
global keeps that shape and `LlvmVarSymbolInfo::ValueType` + `codegen::LoadGlobalConstant` read it as the declared
type. Not done: variant→wider-variant constants; a bare `None` is not comptime (E77, #44).

**Analysis twin** `fn_values::InstantiateFnValue` (instantiates a generic fn named as a value) runs at the same slots
and recurses through tuples/arrays (fixed 2026-10-24, open-bugs #37). A named function's mock accepts only itself in
`AssignableCore` (#35); a closure mock accepts another closure. `let` sets its declared type as the target before
analysing the value (#39), like `ret`/assignment; `cmp` too. A loop's `exit`s (and `else`), a `case`'s branches and an
inferred closure's `ret`s agree on one type through `type_join::TypeJoin` (N9.1, [[type-join-agreed-type]]). A `case` of distinct fns with no target is E1 by design (#43,
The maintainer 2026-10-24: distinct types; declare `FunRef[..]` where it lands; params are not targets). Arguments never see the
caller's target (`WithoutCallersTarget` in function_call_argument_ast.cpp, #40).

**Testing a slot:** gtest only builds + verifies; wrong values and empty fn values need
`tests/codegen/check-coercions.sh` (runs every slot at dev/rel). Add a new slot to both
`test_msc_slot_coercions.cpp` and that check. Probe a fn value by calling it; probe a variant by printing it.

**Alias lowering trap (fixed earlier):** an alias lowered first used to register an empty struct under the target's
name; `EnsureLlvmTypeComplete` now lowers the alias's resolved target. Read llvm types with `GetLlvmType`/`GetLlvmTypeOf`,
never `Sym->LlvmInfo->LlvmType` (null until asked). Related: [[generator-gen-done-sentinel]], [[codegen-checks-suite]].
