---
name: meta-guard-heavy-symmetric
description: "CompilerMetaData is saved only through MetaGuard; any stage that sets a heavy (Enclosing*) field takes a heavy guard, in every stage of that ast"
updated: 2026-10-25
---

`CompilerMetaData::Save` and `Restore` are private (N6.6): the only way to snapshot and put back the context is a
`MetaGuard`, so an error raised part way unwinds it. Nothing re-syncs a depth by hand any more (`Depth()` is gone).

A light guard (`MetaGuard(meta)`) restores every field but the heavy ones: `EnclosingFnScope`,
`EnclosingFnFlavour`, `EnclosingFnRetType`, `EnclosingFnSourceRetType`, `ClosureRetJoin`, `EnclosingFnCmp`, which a
light restore lets persist upwards. **A stage that sets any of those takes `MetaGuard(meta, true)`, and so does every
other stage of that ast that sets one.** A function, coroutine or closure sets them in stage 7, 8 and 11 alike.

**Why:** a light guard around a heavy set lets it escape. Closure stage 8 and function stage 8 set `EnclosingFnScope`
under light guards (found 2026-10-25): after a closure, the enclosing function's later `ret` walked
`CheckLiveUpToFn` past its own scope to the module, as the closure's scope is not on its chain. Latent (the scopes
past a function hold nothing to discharge), but wrong.

**How to apply:** when adding a stage that sets an `Enclosing*` field, use a heavy guard and check the ast's other
stages match. Clearing the assignment target for a subtree is `meta->ClearAssignmentTarget()` (target and type)
inside a guard; `LlvmAssignmentTarget` is cleared separately at the codegen sites. Related:
[[spp-meta-flags-are-per-stage]].
