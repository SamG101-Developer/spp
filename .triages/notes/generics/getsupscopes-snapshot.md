---
name: getsupscopes-snapshot
description: "GetSupScopes returns a SupScopeList (shared snapshot) because a lookup inside a loop over it can rebuild the cache - the old Vec const& was a UAF (blanket-constraint tests, 2026-10-05)"
updated: 2026-10-05
---

`Scope::GetSupScopes()` used to return `Vec<Scope*> const&` into `_SupScopesCache`, on the claim "no caller walks its
super scopes again while holding the reference". False: `SuperClsRefs` loops it and calls `TypeRef::Of` ->
`ReadIn` -> `CanonType` -> `FindInSupScopesByParam` -> `GetSupScopes()` on the same scope after the
`TypeStructureGeneration` moved (lazy attach), and the move-assign freed the vector mid-loop. Showed as a
layout-dependent SIGSEGV in `TestNicheSuperimpositions.test_blanket_sup_with_constraint[_excludes_non_conforming]`
(which of the two crashed changed per build; ASAN pinned it).

Fixed 2026-10-05 in `GetSupScopes` itself (the maintainer: not per caller): the cache is `Shared<Vec<Scope*> const>`, replaced
not overwritten, and the answer is `SupScopeList` (owns the shared ptr; begin/end/size/IsEmpty + implicit
`Vec const&` conversion). A value type, not `*shared_ptr`, because range-for temporary extension (P2718) is not on MSVC.

**How to apply:** never hand out a reference into a cache that a lookup can rebuild; any analysis call can attach
sups ([[sup-attach-fixed-point]]).
