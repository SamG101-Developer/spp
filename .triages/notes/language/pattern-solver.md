---
name: pattern-solver
description: "N9.2 done 2026-10-19 - case patterns go through PatternSolver/MatchPlan; gtest cannot see pattern miscompiles"
updated: 2026-10-19
---

`case` patterns are read once by `analyse/utils/pattern_solver` into a `MatchPlan` (nodes with paths, coverage,
subject edges, ordered `PatternCheck`s: tags then literals). Literal `eq` calls and `== x` comparisons read a hidden
borrow `$_case_subject` (declared before the branches), never a clone of the condition. Bindings still lower to `let`
destructures; moving them onto the plan (P5) was deferred by the maintainer - a move binding must go through the real subject.

**Why:** the gtest suite only builds and verifies IR, never runs it - dropping nested tag checks passed 1460 tests.
**How to apply:** after touching pattern codegen, run `tests/codegen/check-case-patterns.sh` (runtime matching) and
`spp test --all-libs` in a project copy; take the subject's type from stage 7 (`_SubjectType`), as narrowing can make
`self` infer as `Pass[T]` at codegen. A call codegens a clone of its receiver (`InjectReceiver`), so a lowering's
`Clone` must share its analysed lowered form, not deep-copy it (B5, fixed in `?`). Plan and bug list: `.triages/docs_closed/pattern-solver-plan.md`. See `case-pattern-subject-bugs`.
