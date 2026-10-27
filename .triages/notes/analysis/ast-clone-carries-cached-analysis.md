---
name: ast-clone-carries-cached-analysis
description: "Several Clone() impls copy resolved analysis state, so rewriting a clone silently does nothing - build the node instead"
updated: 2026-10-26
---

`Ast::Clone()` is not uniformly structural. `BinaryExpressionAst` copies `_MappedFn` and `Source`; the index, slice and `.res` postfix operators copy their `_MappedFn`; `PostfixExpressionOperatorFunctionCallAst` copies `_OverloadInfo`, `_TransformedAst` and the closure dummies.

**Why:** every one of those was built from the operands *as they were written*. Clone-and-then-rewrite-a-child therefore produces a node that still dispatches through the old mapping — the rewrite has no effect, with no error and no diagnostic. Stage7 sees the mapping is already set and skips rebuilding it.

**How to apply:** when producing a transformed copy of an expression (generic substitution, lowering, any rewrite), construct the node with `MakeShared`/`MakeUnique` from its own fields rather than cloning and assigning over the children. Check the node's `Clone()` first if unsure — a structural one is safe, one with trailing `ast->_Foo = _Foo;` lines is not. Related: `expression-generic-substitution`.

**Desugared nodes (N1.3, 2026-10-13):** a clone carries its lowering and its desugaring markers, so it equals the
original at whatever stage it was taken (async `_TransformedFn`, call `_TransformedLhs`, iterable loop's five
`_Transformed*`/`_Iter*` members, `?`'s `_TransformedExpr`, the else pattern's `_ForIterLoopExit`). Clones after Stage
7 come from generic instantiation, which then swaps in `Source.OriginalImpl` - re-analysis always starts from the
pristine body, never from a clone of an analysed one.

**Settled types too (2026-10-26).** `ArrayLiteralExplicitElementsAst` and `TupleLiteralAst` clones copy
`_InferredType`, the type Stage 7 settled on from the target (`[Some(..), None]` as `[Opt[U64]; 2]`). Without it, a
clone falls back to its first element's type. A `cmp` constant's value reaches codegen through a clone of the
symbol's `CompTimeValue`, so a `None` element was emitted into a `Some` slot as zeros. Only a run shows that
(`check-coercions`' `slots` case); the IR verifies either way.
