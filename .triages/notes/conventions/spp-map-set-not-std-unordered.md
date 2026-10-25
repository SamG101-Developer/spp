---
name: spp-map-set-not-std-unordered
description: "std::unordered_map/set are banned; use spp::Map/Set (ankerl dense), or StableMap/StableSet (ankerl segmented) when references into the container are handed out"
updated: 2026-10-03
---

Never use `std::unordered_map` / `std::unordered_set`. Use the aliases in `headers/spp/utils/types.ixx`:
- `Map` / `Set` = `ankerl::unordered_dense::map/set` (vector-backed: element addresses move on insert).
- `StableMap` / `StableSet` = ankerl `segmented_map/set` (the maintainer added 2026-10-03): elements never move on insert, so use
  these whenever a pointer/reference into the container escapes (interning sets, memo maps returning `const&`).
  Erase is swap-and-pop, so it still invalidates the moved element.

**Why:** the maintainer's rule; ankerl is faster, and the wrapper is `headers/wrappers/ankerl.ixx`. The stable variants exist
because `InternTypeKey` (TypeId = element pointer), `HeadOf`/`ParamsOf` (const& + CachedHead/CachedParams) and
`CompNodeOf` all depend on stable addresses — a plain `Map` there would dangle silently.

**How to apply:** pick `Map`/`Set` by default; `Stable*` only when an address escapes. Never box values in `Unique`
to get stability. Related: [[pure-lookup-blocked-on-typeid]].
