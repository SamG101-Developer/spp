---
name: sroa-skips-sub-byte-coro-index
description: "An elided coroutine frame's i2 suspend index stays in memory (SROA skips sub-byte integers); mem2reg at the peephole points promotes it"
updated: 2026-11-09
---

Coroutines are lowered in the combined module's pipeline, after linking (O10, 2026-11-09): `coro-split` and
`coro-elide` run inside the inliner's CGSCC walk, so a std iterator is inlined before it is split and its frame becomes
an alloca in the caller. SROA then splits that frame, but leaves the suspend index in memory: CoroSplit types it `i2`
(two states), and SROA does not promote an integer narrower than the byte it is stored in (`alloca i8` with `i2`
loads and stores; `opt-23 -passes=sroa` keeps it, `mem2reg` removes it). With a loop body that writes through memory,
the index is stored and reloaded every iteration, and the loop never vectorises: `iter_mut` was 2× C.

**Why:** `RunOptimizationPipeline` adds `PromotePass` (mem2reg) at every peephole point (`registerPeepholeEPCallback`,
O1+). Those come after each InstCombine of the inliner's function walk, where the frames are elided, early enough for
the loop passes. Added later (vectorizer-start) or at scalar-optimizer-late, the loop still did not vectorise. With it,
`iter_mut` runs at 0.6× C.

**How to apply:** keep the callback while coroutines are lowered in the pipeline. If a generator loop does not
vectorise, look for a surviving `alloca` with `i2` accesses in the caller. Moving lowering back before linking would
undo O10: a coroutine from another module could no longer be elided.
