---
name: free-functions-walk-in-reverse
description: Free functions are walked last-written-first in every stage; Stage 7 folds analyse the callee ahead (AnalyseAhead)
updated: 2026-10-26
---

`FunctionPrototypeAst` Stage 1 replaces each free function with a mock `sup $f ext FunRef...` block inserted at the
**front** of its module's (or sup's) `Members` (`function_prototype_ast.cpp`, the `Members.insert(...begin()...)`
sites). Each one is prepended in turn, so the functions end up in reverse source order, and every later stage walks
them in that order: `g, twice, main` is analysed `main, twice, g`. The user's module was also analysed before std in a
probe.

A comp-time fold run during Stage 7 (`[x; n]` sizes, comp generic arguments via `comp_generics::FoldCompCall`) calls
`EvaluateCompTime` on the callee's body. If the callee's Stage 7 has not run yet, nodes that desugar in Stage 7
are empty. `BinaryExpressionAst::_MappedFn` is null, so the fold segfaults in `PostfixExpressionAst::EvaluateCompTime`. A `cmp
fun` written *after* its use works and one written *before* crashes (`cmp fun twice() -> USize { ret 2_uz * 2_uz }`
then `fun main() -> Void { let a = [1; twice()] }`). Calling the method directly (`2_uz.mul(2_uz)`) does not crash,
because the call's Stage 9 analyses an unanalysed call itself.

Fixed 2026-10-25 (N7.9): the fold's call (`PostfixExpressionOperatorFunctionCallAst::EvaluateCompTime`) calls
`FunctionPrototypeAst::AnalyseAhead`. That walks the prototype from its mock `sup` scope with a fresh metadata, so its
body scopes are built where the walk would build them. The Stage 7 walk then skips the prototype
(`_SkipIfAnalysedAhead`). A body that folds a call to itself raises E3.

**Why:** a fold assumes the callee's body is analysed, and the walk order doesn't make that true.

**How to apply:** any new Stage 7 fold site goes through the call's Stage 9 (or `comp_generics::FoldCompCall`), so it
gets the callee analysed. Don't assume source order when reasoning about which body a stage reaches first. The Stage 9
walk that waited for callee bodies is gone (N7.11): see [[stage-9-walk-removed]].
