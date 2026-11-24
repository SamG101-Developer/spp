---
name: spp-soundness-audit-2026-09
description: "2026-09-27 hole-hunt (~95 tests, red ones for open bugs): its linear checks are gone with affine ownership; what still holds from it"
updated: 2026-11-24
---

Superseded in part by affine ownership (2026-11-23, [[affine-ownership]]): the linear checks this audit tuned (the
branch-consumption check in `linear_utils::IsLive`, and the reverted assign-over-live-value `CheckOverwrite`) are
deleted with `linear_utils`. A value held on only some paths is now dropped on each path still holding it, and an
overwrite drops the old value first.

A probe-driven audit on 2026-09-27 added ~95 tests (red ones document open bugs; fixed ones carry `// FIXED`). What
still holds from it:

- A generic instance with a `Copy` sup is `Copy` only when all its attributes are.
- Loop exit states are merged (2026-09-28): every `exit` snapshots into `CompilerMetaData::LoopExitStates` for its
  target loop, and `LoopConditionalExpressionAst::Stage8` joins all paths out as `case` branches are.
- Pattern-guard moves were E114 from here; since 2026-11-24 a guard may move a value, and E114 is only for one moving
  its branch's own bindings or the subject.
- Polymorphic recursion still runs out of memory (its tests are `DISABLED_`). The open-bug list is
  `../../../.triages/open-bugs.md`.

**Why:** the audit's red tests and fixes are spread across the suites; this says which of its rules outlived the
linear checks.

**How to apply:** check std has been synced into the test fixture before trusting a suite run
([[spp-test-harness-stale-std]]). Related: [[add-tests-even-if-failing]], [[spp-probe-harness]].
