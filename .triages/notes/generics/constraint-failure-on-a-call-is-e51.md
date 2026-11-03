---
name: constraint-failure-on-a-call-is-e51
description: "A call whose only candidate fails a generic constraint raises E51 (no valid signatures) with E78 inside; E78 itself is raised for class, alias and sup constraints"
updated: 2026-11-04
---

When a function call's candidate fails a generic constraint, overload resolution collects that as the candidate's
reason and raises `SppFunctionCallNoValidSignaturesError` (E51), with the `SppGenericConstraintError` (E78) printed
inside it, even when there is only one candidate. E78 is the top-level error only where no overload resolution runs:
a class, alias or `sup` constraint (`B[U32]()` against `cls B[T: A]`, `type C[U] = B[U]`, `sup [X] B[X]`).

A pack's constraints are no different: an element failing `..Ts: C`, or a pair failing a pairwise `..Ts: Add[Ts]`
([[pack-constraints-pairwise]]), makes the call E51 too
(`TestVariadicPack.test_invalid_binary_fold_over_a_pairwise_addable_type_pack_of_mixed_types`).

**Why:** found 2026-11-02 when a variadic pack test expected E78 from `takes_pair(b)` and got E51 (the test is now
`test_invalid_pack_passed_whole_to_a_tuple_constrained_function`, after the pack redesign made a whole pack E123).
7 existing tests of a failing constrained call expect E51.

**How to apply:** a `SPP_TEST_SHOULD_FAIL_SEMANTIC` for a call that fails a constraint expects
`SppFunctionCallNoValidSignaturesError`. Since E51 is also what most other bad calls raise, a test meant to prove the
constraint (rather than something else) failed needs a passing twin that differs only in what meets the constraint.
