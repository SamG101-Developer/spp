---
name: variant-drop-is-a-no-op
description: "Var has no parts, so drop code reads variants through VariantMemberRefs: NeedsDrop recurses into members, EmitDrop switches on the tag (fixed 2026-11-10)"
updated: 2026-11-10
---

`std::variant::Var[..Variants]` is an empty class; its members exist only in the lowered `{ tag, payload }`. Every
drop path reads a type's parts through `type_members::GetAllParts`, which finds none for a variant, so:
- `drop_utils::NeedsDrop(Vec[S32] or Bool)` is false, and `EnsureDropInstantiated` never walks the members;
- `codegen::EmitDrop` (`llvm_drop.cpp`) handles generators, own `drop`s and parts, but has no variant branch, so
  `std::mem::ops::drop[T=Bool or Vec[S32]]` compiles to a store and `ret` (seen in both dev and rel IR).

The analysis still treats the variant as linear (`IsCopyable` is false, E94 without a `drop`), so the program is
forced to call a `drop` that does nothing. Was found by valgrind on
`std::tst::variant::test_a_variant_can_hold_a_non_copyable_alternative` (4 bytes definitely lost in rel; dev hides it
because a stale copy of the pointer survives on the stack). The same goes for a variant nested in a struct or tuple,
and for `Opt[T]`/`Res[T, E]` over owning types.

**Fixed 2026-11-10:** `NeedsDrop` and `EnsureDropInstantiated` (`drop_utils.cpp`) recurse into
`type_members::VariantMemberRefs`, and `EmitDrop` loads the tag (`LoadVariantTag`) and switches to an `EmitDrop` of
the payload (`GetVariantPayloadPtr`) for each member that needs one; members are in tag order. A partial move out of a
variant is an ICE there (a variant has no parts). Verified: valgrind conformance 663/666 (the rest are
[[method-result-receiver-temporary-unchecked]]), no invalid accesses; 1285 targeted gtests pass.

**Why:** anything new that walks a type's parts (`GetAllParts`) misses variants the same way.

**How to apply:** code that asks "what does this type own" must handle `IsTypeVariant` beside `GetAllParts`. A main-only valgrind probe does not
show variant leaks; probe through `spp test --filter ... -m rel --runner valgrind`.
