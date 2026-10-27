---
name: variant-pack-flattens
description: "A variadic pack inside a variant flattens into its members: f[..Rest](x: Bool or Rest) with f[S32, Str] is Bool or S32 or Str"
---

Decided by the maintainer 2026-10-22: a pack among a variant's members flattens, so `fun f[..Rest](x: Bool or Rest)` called as
`f[S32, Str]` takes `Bool or S32 or Str`, never `Bool or (S32, Str)` (the tuple a variadic otherwise becomes in an
instance - see [[variadic-pack-model]]). Inference matches the variant whole: `Rest` takes the members the fixed
ones leave (`substitution_unify.cpp`, the pack-aware branch).

**Why:** "otherwise we just put the tuple into the generic" - flattening is the useful meaning.

**How to apply:** keep pack-in-variant results flat in substitution and inference; a tuple member appearing in such a
variant is a bug. Open as of 2026-10-22: an explicit `f[S32, Str](true)` does not widen `true` into the flattened
variant (red test `TestGenericInference_Variant.test_valid_explicit_variant_pack_argument`).
