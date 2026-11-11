---
name: expect-lowered-before-inlining
description: "llvm.expect must be emitted at the call site (CallSiteImpl), in the same block as the branch it feeds; a wrapper body or a block in between drops it"
updated: 2026-11-06
---

Every `!intrinsic` lowering is a real LLVM function (`function_implementation_lowered_ast.cpp`) that call sites
`call` and the optimiser inlines later. In LLVM 23's default pipeline (`PassBuilderPipelines.cpp`,
`buildModuleSimplificationPipeline`) the early per-function cleanup runs `LowerExpectIntrinsicPass` (line ~1059)
before `AlwaysInlinerPass` (line ~1210). An `llvm.expect` in the wrapper is lowered while it has no branch to annotate,
and is dropped; the caller never sees the hint. `LowerExpect` also runs before SROA, so an expect whose result reaches
the `br` through an alloca (`let c = likely(x)`) is missed too.

**It must also be in the branch's own block.** Per block, `LowerExpect` first weights that block's terminator, then
replaces every expect in the block with its argument. An expect in block A feeding a `br` in a later block B is gone
before B is reached. `case` used to compute its condition and then `br case.entry`; that block was removed
(2026-11-06) so the branches start where the condition ended. `loop` and guards already branch in the condition's block.

Instructions that survive inlining (inline asm, `llvm.assume`, `unreachable`) are fine in a wrapper:
`std::hints::black_box` is one.

**Why:** Rust hit the same; its `likely` now leans on `cold_path`, handled in rustc's own codegen before LLVM.

**How to apply:** `std::hints::likely`/`unlikely` are the model: `LoweredFnImpl::CallSiteImpl` (`builtins.cpp`,
`fn_impls::StdHintsExpect`) lowers a direct call in place, in `PostfixExpressionOperatorFunctionCallAst::Stage11_CodeGen`;
the wrapper (`StdHintsBranch`) only passes the value through, for indirect calls. A new branch hint goes the same way,
and a new construct that branches on a user condition must branch in the block the condition ends in.
`tests/codegen/check-branch-hints.sh` checks the weights survive to `lto.ll`.
