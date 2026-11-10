---
name: variant-drop-is-a-no-op
description: "OPEN 2026-11-10: dropping a variant destroys nothing - Var has no parts, so NeedsDrop says false and DropValue has no tag switch; Vec in a variant leaks"
updated: 2026-11-10
---

`std::variant::Var[..Variants]` is an empty class; its members exist only in the lowered `{ tag, payload }`. Every
drop path reads a type's parts through `type_members::GetAllParts`, which finds none for a variant, so:
- `drop_utils::NeedsDrop(Vec[S32] or Bool)` is false, and `EnsureDropInstantiated` never walks the members;
- `codegen::DropValue` (`llvm_drop.cpp`) handles generators, own `drop`s and parts, but has no variant branch, so
  `std::mem::ops::drop[T=Bool or Vec[S32]]` compiles to a store and `ret` (seen in both dev and rel IR).

The analysis still treats the variant as linear (`IsCopyable` is false, E94 without a `drop`), so the program is
forced to call a `drop` that does nothing. Found by valgrind on
`std::tst::variant::test_a_variant_can_hold_a_non_copyable_alternative` (4 bytes definitely lost in rel; dev hides it
because a stale copy of the pointer survives on the stack). The same goes for a variant nested in a struct or tuple,
and for `Opt[T]`/`Res[T, E]` over owning types.

**Why:** the leak looks like a std bug but std's code is correct.

**How to apply:** the fix is `NeedsDrop` (and `EnsureDropInstantiated`) recursing into
`type_members::VariantMemberRefs`, and `DropValue` loading the tag (`LoadVariantTag`) and switching to a per-member
`DropValue` on the payload (`GetVariantPayloadPtr`) for each member that needs it. A main-only valgrind probe does not
show variant leaks; probe through `spp test --filter ... -m rel --runner valgrind`.
