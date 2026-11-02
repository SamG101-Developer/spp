---
name: constraint-failure-on-a-call-is-e51
description: "A call whose only candidate fails a generic constraint raises E51 (no valid signatures) with E78 inside; E78 itself is raised for class, alias and sup constraints"
updated: 2026-11-02
---

When a function call's candidate fails a generic constraint, overload resolution collects that as the candidate's
reason and raises `SppFunctionCallNoValidSignaturesError` (E51), with the `SppGenericConstraintError` (E78) printed
inside it, even when there is only one candidate. E78 is the top-level error only where no overload resolution runs:
a class, alias or `sup` constraint (`B[U32]()` against `cls B[T: A]`, `type C[U] = B[U]`, `sup [X] B[X]`).

**Why:** found 2026-11-02 when `TestVariadicPack.test_invalid_pack_passed_whole_fails_its_constraint_when_called`
expected E78 from `takes_pair(b)` and got E51. 7 existing tests of a failing constrained call expect E51.

**How to apply:** a `SPP_TEST_SHOULD_FAIL_SEMANTIC` for a call that fails a constraint expects
`SppFunctionCallNoValidSignaturesError`. Since E51 is also what most other bad calls raise, a test meant to prove the
constraint (rather than something else) failed needs a passing twin that differs only in what meets the constraint.
