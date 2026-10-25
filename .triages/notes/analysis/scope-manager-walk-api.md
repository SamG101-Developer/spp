---
name: scope-manager-walk-api
description: "ScopeManager is only the walk (N1.10, 2026-10-13): NewManagerAt (always positioned), ScopePositionGuard, SkipSubtree; ScopeTree owns per-compile state"
---

Since N1.10 (2026-10-13), `ScopeManager` is only the walk over a compile's `ScopeTree`:

- New managers come from another manager: `NewManagerAt(scope)`, always positioned there (read or walk), or
  `NewManagerAt(ScopePosition)` to resume a walk. The constructor is private; the root is `ScopeManager::ForTree`.
  Never `ScopeManager(...)` by hand. Positioning is free: `ScopeIterator` keeps its frames in a `SmallVec<Frame, 8>`
  (copies defined out of line in `scope_iterator.cpp`, the one TU importing `gch` for it).
- A detour on the same manager is `ScopePositionGuard` (restores on exit, exceptions included); from a `catch`, use
  `Position()` / `ReturnTo()`. Stage starts are `RestartWalk()`.
- `SkipSubtree(scope)` replaced `ExhaustScope`/`SkipPastScope` and always leaves `CurrentScope` on `scope`.
- `ScopeTree` (owned by the compiler) holds the global scope, `NormalSupBlocks`, `GnSupBlocks`, `TempScopes`; its
  destructor clears the per-compile caches (no `ScopeManager::Cleanup` any more). `Scope::Tree()` reaches it.
- Sup attachment lives in `analyse::utils::sup_attachment`; the `Self` symbol helpers in `self_type`.

**Why:** temp managers were built three ways, detours restored by hand (not exception-safe), statics leaked
between compiles, and two skip operations disagreed on where they left you. **How to apply:** use these names in
new code. Related: [[analyse-utils-module-map]], [[exhaust-scope-current-scope]].
