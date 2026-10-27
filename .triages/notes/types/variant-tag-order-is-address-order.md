---
name: variant-tag-order-is-address-order
description: "Variant tag numbers follow the identity's member order; FIXED 2026-10-05 - StableKeyLess orders by symbol Name + TypeSymbol::Serial, not addresses. Why not FqName, and how to read tag-only IR diffs"
updated: 2026-10-05
---

Variant tags come from `codegen::GetVariantIndexOfMember` -> `type_members::VariantMemberRefs`, which reads the
identity's member order (`HeadOf(id).Members`). Until 2026-10-05 `VariantKey` (scope.cpp) and the substitution
rewriter (instance_key.cpp `Tag::Variant`) sorted members by raw key words, i.e. **symbol addresses**: consistent within
one compile (not a miscompile), but any allocation change renumbered tags, so IR diffs showed tag-only changes.

**Fixed 2026-10-05:** both sort with `scopes::StableKeyLess` (instance_key): symbol words by `Name->ToView()` then
`TypeSymbol::Serial` (a creation counter, fresh in the copy ctor), nested `TypeId`s / `CompId`s recursively, spelled
tags (`Name`, `Unresolved`, `TypeBound`) by text, the rest by value; raw words only as the last tie-break so the order
stays TOTAL (else `S32 or Str` != `Str or S32`, the bug the earlier generic work fixed).

**Not `FqName()`:** it is rebuilt on scope re-parenting (`ScopeLinkageGeneration`), so a variant keyed before and after
could sort two ways and split into two identities; and it reads `Alias->Resolved`, null while keying an alias.

**How to apply:** a tag-only IR diff now means a member order really changed (a rename, a new type), not allocation.
Widening between variants often became a straight tag copy (no `select` remap) since both number by name. Tests:
`test_msc_variant_member_order.cpp`. Related: [[spp-ir-diff-verification]].
