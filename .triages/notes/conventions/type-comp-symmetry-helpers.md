---
name: type-comp-symmetry-helpers
description: "Type and comp generics must be handled symmetrically; factor the comp mirror of a type path into a shared helper, never a hand-rolled copy"
updated: 2026-09-30
---

Anywhere the generic system handles type generics, it should handle comp generics too, and the reverse (about 95% of cases). When adding the comp side, write helper functions that mirror the type-side ones rather than repeating code inline, e.g.:
- `packs::PackElementValues` mirrors `PackElementTypes`;
- `scopes::CompParamIdOf` mirrors `ParamIdOf`;
- `packs::IsPackParam` covers both kinds.

Also make the comp rule match the type rule exactly. A stricter comp rule is itself an asymmetry, and it broke std: comp identity-only matching vs type's "identity, else spelling".

**Why:** the user flagged it ("if you find yourself writing repeated code for these symmetries, add helper functions") during the 2026-09-30 type/comp symmetry audit.

**How to apply:** audit doc foundation 7 lists the gaps. Before adding a comp branch, look for the type-side helper to mirror or generalise. Add unit tests once fixes are in. Related: [[variadic-pack-model]].
