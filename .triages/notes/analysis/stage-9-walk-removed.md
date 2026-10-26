---
name: stage-9-walk-removed
description: No Stage 9 walk since N7.11; constants fold in Stage 7 (CmpStatementAst::AnalyseAhead); Stage9 methods are the evaluator
updated: 2026-10-26
---

Since 2026-10-26 (N7.11) there is no Stage 9 walk over the modules: no `kCompTimeResolve`, no
`CompilerBoot::Stage9_CompTimeResolve`, and no overrides on declaration nodes. The remaining methods were renamed
`EvaluateCompTime` (declared and defined after `Stage11_CodeGen`), as the comp-time **evaluator** that folds call (`FoldCompCall`, a call's Stage 9, the `cmp` fold). The
evaluator walks a `cmp fun` body, so `TypeStatementAst::EvaluateCompTime` stays, to step over a local `type` statement's scope.
After Stage 8 comes Stage 9.5 (monomorphisation).

The walk's three jobs moved into Stage 7:
- **Constants:** `CmpStatementAst::_AnalyseValue` analyses the value and then folds it, putting the result on the
  symbol (`CompTimeValue`) for any later read. It keeps the folded value in `_FoldedValue` until Stage 8 has checked
  the analysed one, because Stage 8 walks the scopes the analysis built (a `case`), and only then replaces `Value`.
  Until a constant's Stage 7, its symbol holds the value as written in Stage 2, never analysed. Folding that segfaulted
  (`[1; b]` with `cmp b = 1_uz + 2_uz`, in either order). So a read (`IdentifierAst::EvaluateCompTime`, a static member
  access) calls `VariableSymbol::Stmt->AnalyseAhead` first. That runs the statement's Stage 7 on a manager standing on
  its own scope, with fresh metadata, and the walk skips it later. A cycle (`cmp a = 1_uz + a`) is E3, from the
  in-progress state.
- **Unused comp defaults** fold in `GenericParameterAst::Stage7`, to prove them constant.
- **Annotation target masks** are checked in `AnnotationAst::Stage7` (`_CheckTarget`). The mask is read by its comp
  key (`FoldedCompAstOf`), not by running the `|` call. Running the call ICE'd in practice: `!always_inline` on
  `S32::bit_ior` is checked as that method's own Stage 7 begins, so the fold reached a prototype still being analysed
  (E3 on std's `!annotation(target=...)`).

**Why:** the walk existed only so that callee bodies and constants were analysed before anything folded them.
`FunctionPrototypeAst::AnalyseAhead` and `CmpStatementAst::AnalyseAhead` now do that on demand.

**How to apply:** new comp-time work goes in Stage 7 at the node that needs the value, through the evaluator. A read
of anything declared elsewhere goes through its `AnalyseAhead`. Don't fold anything a node's own Stage 7 is in the
middle of. When evaluating while the reached code may still be analysing, prefer the key fold (`FoldedCompAstOf`):
it needs no analysis.
