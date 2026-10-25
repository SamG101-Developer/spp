______________________________________________________________________

## name: spp-soundness-audit-2026-09 description: 2026-09-27 hole-hunt - the branch-consumption check is ON (std leaks fixed in project/vcs/std only), the assign-overwrite check is still reverted, and where the red tests live updated: 2026-09-27

A probe-driven audit on 2026-09-27 added ~95 tests (red ones document open bugs; fixed ones carry `// FIXED`).

**Branch consumption check - enabled (same day, follow-up):** `IsLive` (`linear_utils.cpp`) treats
`IsInconsistentlyMoved`/`Initialized` as still owned, and `ValidateInconsistentMemory` (`case_utils.cpp`) now counts
the fall-through path of an `else`-less `case` and treats a branch ending in a `!` call as divergent. Never use
`IsInconsistentlyPartiallyMoved` for it (compares move-site pointers; two branches taking the same field "differ").
It exposed ~20 real std leaks (fs/file, fs/path, option, result, iterator adaptors, range) plus `Iterator::step_by`
yielding everything. Those std fixes were merged to std master (ecb6981) and pulled into `tests/test_outputs/vcs/std` the same day. A generic `T` cannot bind a borrow, so std got
an overload `drop[T](val: &T)` (no-op) for adaptors over `iter_ref`.

**Assign-over-live-value check - still reverted:** `linear_utils::CheckOverwrite` works (used for same-scope
shadowing), but in `AssignmentStatementAst::Stage8` it breaks ~43 tests that use `let mut x = f(1); x = 2` as a
type-check idiom (and `S32 or Bool` is not `Copy`). A semantics decision for the maintainer.

**Why:** knowing which fixes are gated on std or on test idioms avoids re-deriving it. **How to apply:** check whether
std has been synced into `tests/test_outputs` before trusting a suite run; to test compiler changes against the
project's std without touching the fixture, run `spp_tests` from `<dir>/w1/w2` with a copied `<dir>/tests/test_outputs`.

Fixed later the same day: loop dataflow (zero-iteration merge, and skip-moves via the `LoopSkipMoves` meta field),
recursive types through tuple/generic/variant (`HoldsByValue`), `Copy` over non-Copy attributes, pattern-guard moves
(new E114), tuple aliases, and `f.await` carrying a generator's borrows. Semantics chosen: a generic instance with a
`Copy` sup is Copy only when all its attributes are. Still open: linear temporaries (`test_lin_temporaries.cpp`) and
polymorphic recursion OOMs (DISABLED\_). Loop exit states were merged 2026-09-28: every `exit` snapshots into
`CompilerMetaData::LoopExitStates` for its target loop, and `LoopConditionalExpressionAst::Stage8` merges all paths out
like case branches. The open-bug list lives in `../../../.triages/open-bugs.md`.
Related: \[[spp-linear-types]\], \[[add-tests-even-if-failing]\], \[[spp-probe-harness]\].
