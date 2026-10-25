---
name: defer-applies-to-earlier-exits
description: "Fixed 2026-09-22 - Scope::Deferred must be cleared on Stage 8 scope entry, or a loop body's 2nd pass runs a defer at an exit above it (E99 false positive)"
updated: 2026-09-22
---

Stage 8 registers a `defer` in `Scope::Deferred` where it is reached (`DeferStatementAst::Stage8_CheckMemory`), so an
exit written above it does not run it. But `LoopConditionalExpressionAst::Stage8_CheckMemory` walks the body twice
against the *same* scope, and `Deferred` was never cleared: pass 2's early `exit` saw pass 1's defer, which pass 1's
closing `}` had already "run" (`MovedBy(exit_point)`) -> E99 on valid code (examples socket.spp, `else { exit }` above
`defer drop(msg)`). Fix: `InnerScopeExpressionAst::Stage8_CheckMemory` clears `Deferred` on entry, mirroring
codegen's `DeferredReached.Clear()`. Test: `DeferStatementAst.test_valid_loop_exit_above_defer_does_not_run_it`.

E99 also used to underline the move location as if it were the exit; it now takes the exit point, with the move and
the defer as context blocks.
