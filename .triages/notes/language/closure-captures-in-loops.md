---
name: closure-captures-in-loops
description: "Closure captures vs the loop body's double Stage 8 pass - both bugs fixed (capture re-init in f692668d); tests must reach pass 2, so no \"exit"
---

Loop bodies are memory-checked at least twice against the same scopes (since N8.6, 2026-10-17: pass 1 from the entry state, then from the join of the ways back round - body end and every `skip` - until it settles; std takes 2-3 passes) ([[defer-applies-to-earlier-exits]] is the same
mechanism). Two capture bugs came from that, found 2026-09-22 via examples socket.spp:

- **Fixed (f692668d):** a closure's own copy of a capture was never re-initialised in Stage 8 (parameters are, via
  `FunctionParameterAst::Stage8_CheckMemory` -> `InitializedBy`), so a move-capture consumed in a closure in a loop read
  as moved on pass 2 (false E5). `ClosureExpressionCaptureGroupAst::Stage8_CheckMemory` now calls `InitializedBy` on
  the inner symbol. **A loop test needs a second pass:** since 2026-09-27 a body that diverges without a `skip` is
  checked once, so a body ending in `exit` never exercises this. Use `loop b` with a `b: Bool` parameter.
- **Fixed:** that function recorded the outer symbol's move as `{this}` (the group). On pass 2
  `ClosureExpressionParameterAndCaptureGroupAst` lends the captures out to a temporary arg group, leaving nulls, so the
  E5 formatter's `group->PosEnd()` segfaulted. Now recorded against `cap.get()`.
