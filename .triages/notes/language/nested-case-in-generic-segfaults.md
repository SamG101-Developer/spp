---
name: nested-case-in-generic-segfaults
description: "Fixed - a case with no else was inferred as Never when its branches all diverged, so the fall-through block got unreachable"
---

Fixed 2026-09-05. `case a { case b { abort() } }` segfaulted whenever `b` was false. Nothing to do with generics,
despite the name of the test that found it.

**Why:** `CaseExpressionAst::InferType` returned the branches' common type. With one branch that aborts, that is
`Never`, so the *outer* branch's `body_is_never` check in `case_expression_branch_ast.cpp` believed its body could not
complete and terminated the inner case's end block with `unreachable`. But a case with no `else` finishes without
running any branch at all - that end block is reached on exactly the path the guard exists to allow. Fix is one early
return in `InferType`: no `else` means the value is `Void`, never `Never`. A case that *does* have an `else` and whose
branches all diverge is still `Never`, which is why "both branches abort" must keep aborting - test both.

**How to apply:** this is also half of the standing Todo in that function about statement-position cases yielding
`Void`. The other half (a case *with* an else, in statement position) still yields its branch type; the discard is
caught separately by `expr_utils::ValidateDiscardedValue`, which recurses into the branches rather than looking at the
case's own type.
