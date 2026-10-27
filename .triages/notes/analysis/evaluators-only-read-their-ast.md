---
name: evaluators-only-read-their-ast
description: EvaluateCompTime must never write results into its own AST; a cmp fun body is shared by every call and by codegen
updated: 2026-10-26
---

An `EvaluateCompTime` builds its result into `meta->CompTimeResult` and leaves its own AST as it found it. Until
2026-10-26, three evaluators wrote the folded elements back: `ArrayLiteralExplicitElementsAst`
(`Elems[i] = AstClone(...)`), `TupleLiteralAst` (the same), and `ArrayLiteralRepeatedElementAst` (`Elem = ...`).

Inside a `cmp fun`, the evaluator runs on the function's one body. So the first call's values became the body:
- `f(1)` then `f(2)` returned `f(1)`'s tuple;
- worse, Stage 11 then compiled the rewritten body, so a run-time `pair(5)` printed `1, 2` in dev and rel.

Values that belong to one evaluation live in symbols (`CompTimeValue`), and `FunctionImplementationAst` saves and
restores them per call.

**Why:** the body is shared by every comp-time call and by code generation. Only a symbol's value is per call.

**How to apply:** a new `EvaluateCompTime` reads its children and returns a fresh node. Writing a fold into the AST is
for analysis only (Stage 7, for example `ArrayLiteralRepeatedElementAst`'s `Size`), and never for something a
`cmp fun` body can contain. `tests/codegen/check-comp-eval.sh` runs a body at run time after a comp-time call.
`TestCompTimeValues.test_*_in_a_function_body_is_evaluated_per_call` calls one twice.
