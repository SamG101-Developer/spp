---
name: expect-lowered-before-inlining
description: "llvm.expect inside an !intrinsic wrapper is lost: LowerExpectIntrinsic runs before the always-inliner, so likely/unlikely must be emitted at the call site"
updated: 2026-11-02
---

Every `!intrinsic` lowering is a real LLVM function (`function_implementation_lowered_ast.cpp`) that call sites
`call` and the optimiser inlines later. In LLVM 23's default pipeline (`PassBuilderPipelines.cpp`,
`buildModuleSimplificationPipeline`) the early per-function cleanup runs `LowerExpectIntrinsicPass` (line ~1059)
before `AlwaysInlinerPass` (line ~1210). An `llvm.expect` in the wrapper is lowered while it has no branch to annotate,
and is dropped; the caller never sees the hint. Under LTO the std module is optimised before linking, so it is worse.
`LowerExpect` also runs before SROA, so an expect whose result reaches the `br` through an alloca is missed too.

Instructions that survive inlining (inline asm, `llvm.assume`, `unreachable`) are fine in a wrapper:
`std::hints::black_box` is one.

**Why:** Rust hit the same; its `likely` now leans on `cold_path`, handled in rustc's own codegen before LLVM.

**How to apply:** a branch hint (`likely`/`unlikely`, `expect.with.probability`) is emitted at the call site, with the
expect's result feeding the branch directly; do not add it as a wrapper body in `fn_impls`.
