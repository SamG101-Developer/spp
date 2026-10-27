---
name: call-args-replaced-after-analysis
description: "Since the call-check rewrite (2026-09-28) the analysed arg values are MOVED into the elaborated args (ElaborateCall), never copied/freed; the old swap freed them and scopes dangled - the intermittent CheckLiveUpToLoop segfault"
updated: 2026-09-28
---

`PostfixExpressionOperatorFunctionCallAst::Stage7` swaps its arguments for the matched overload's copies:
`FnArgGroup->Args = std::move(overload.FnArgs->Args)`. `DetermineOverload` clones the argument group once per candidate,
and the copies carry the `self` convention that matching set. Stages 8 and 11 therefore run on **copies**, while every
scope created while analysing the **originals** (an argument `loop`, a closure, a block) names the original as its
`AstNode`.

**The bug (2026-09-28):** the originals were freed by that swap. `CheckLiveUpToLoop` reads each ancestor scope's
`AstNode` (via `AstAs<LoopConditionalExpressionAst>`), so an `exit` inside `h(loop true { .. exit v })` hit freed memory.
- It crashed about 1 run in 6, and only under `spp_tests`, never `spp` (allocation-pattern dependent).
- It showed as "killed by signal" with no output, and passed on some full runs and failed on others.
- The debug build shows it deterministically: the loop's `SPP_ASSERT(sm->CurrentScope == _Scope)` fails, because the
  copy's `_Scope` was null.

**Superseded design (2026-09-28, call-check rewrite, unverified by the suite yet):** candidates no longer clone
the args. `CheckCandidate` records `ParamSlot`s; `overload_resolution::ElaborateCall` moves each analysed `Val` into the
winner's keyword args, so the scopes' `AstNode`s stay valid and `_AnalysedArgs` is gone. The one place a value is still
replaced is forwarding, and `BuildFwdCall(Unique&&, ...)` moves the value into the forwarding call rather than cloning
it. If this crash class reappears, check that nothing in elaboration drops a `Val`.

**Original fix:**
- The call keeps the originals in `_AnalysedArgs`, purely to keep them alive.
- `LoopConditionalExpressionAst::Clone` now carries `_Scope`, as `InnerScopeExpressionAst` and the prototypes already do.

**How to apply:** a crash that only some full runs hit, with no output, is a use-after-free until proven otherwise.
- Loop the single test (`--gtest_filter`) 10-30 times to measure the rate.
- Then run the debug-asan `spp_tests`: its asserts often pin the cause faster than the ASAN report.
- A scope-owning AST whose `Clone()` drops `_Scope` is a smell.

Related: [[asan-build-for-llvm-crashes]], [[ast-clone-carries-cached-analysis]], [[scope-defined-in-can-dangle]].
