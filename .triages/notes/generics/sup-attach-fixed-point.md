---
name: sup-attach-fixed-point
description: "Sup attachment is a memoised lazy query since 2026-09-15; inline constraint checks against a half-built graph were the old module-order bug, and the deferral/prune machinery is gone"
updated: 2026-11-01
---

Since 2026-09-15 (design change 5, `.triages/docs_closed/generic-design-roadmap.md`), `ScopeManager::AttachSpecificSuperScopes`
attaches a scope's sups **once** (`Scope::SupsAttached`, set *before* attaching so a cycle sees a partial list instead
of recursing) and checks each generic constraint **inline**. While `AttachAllSuperScopes` sweeps,
`Scope::OnSupScopesRead` (called from `GetSupScopes()` and the two sup-search helpers in `scope.cpp`) attaches a scope's
sups the first time anything reads them, so a constraint is never checked against a half-built graph.
`DeferredSupConstraint` and `PruneUnsatisfiedSupConstraints` are gone. The sweep still repeats to a fixed point:
attaching a constrained sup instantiates the substituted type, creating scopes the sweep must visit.

**Why:** checking inline *without* the lazy read failed std in 13/13 module orders (E62 `view.spp:260`, E51, E26, the
error moving with the order). That was the old CI-only bug: 3 of 8 orders failed before the 2026-09-07 deferral fix.
With the lazy read, 13/13 orders pass and the IR and object symbols are identical to the deferred version. That 13/13 no longer holds: see [[module-order-failures]].

**Mocks attach on first read** (2026-11-01, `Scope::SupsDeferred`): the sweep marks a `$` mock and skips it, because 40,554 of 42,229 were never read, and `Scope::OnDeferredSupsRead` attaches one when it is. Only the sweep sets the flag: a mock cloned after stage 5.5 is never attached, as before. `./project` went from 3.04 s to 2.86 s (median of 40 pairs, 95% range 0.115-0.220 s) with identical IR.

**How to apply:**
- The hook captures the stage's `meta`; a guard in `AttachAllSuperScopes` clears it on every exit, because the test
  harness runs many compilations in one process. Never leave it installed.
- The memo is a flag on the `Scope`, not a `Set<Scope*>`: scopes are freed in Stage 9.5 and addresses are reused
  ([[scope-defined-in-can-dangle]]). A copy (clone) does not inherit it.
- A new direct read of `DirectSupScopes`, at any stage, bypasses the hooks and can see a deferred mock's empty list; go through `GetDirectSupScopes()` or `GetSupScopes()`.
- To prove an order bug, add a temporary seeded `std::ranges::shuffle` to `utils::files::GlobSpp` behind an environment
  variable and sweep seeds (reordering files on disk does nothing, since `GlobSpp` sorts). Compare the generated IR
  with [[spp-ir-diff-verification]].

See [[e60-double-extension-order-dependent]], [[spp-perf-hot-path]].
