---
name: kcfi-bundles-not-metadata
description: "S9.1 CFI tags calls with kcfi operand bundles at emission; custom call metadata is dropped by the optimiser; functions are typed before it"
updated: 2026-11-08
---

S9.1 (2026-10-18): indirect calls carry a `kcfi` operand bundle from where they are emitted (`CfiCallBundles` in
llvm_fn.cpp), never custom instruction metadata: SimplifyCFG sinking, GVN PRE and similar merges drop metadata kinds
LLVM does not know (30 of 364 calls lost an `!spp.cfi` tag). Function-level metadata (`spp.cfi`, `spp.cfi.coro`) does
survive, including into CoroSplit's `.resume/.destroy/.cleanup` clones, so `ApplyCfiTypes` types functions from it.
A bundle on `llvm.coro.resume`/`destroy` is accepted by the verifier and kept by coro lowering.

Function-level metadata does **not** survive a pass that rebuilds a function: a MergeFunctions thunk is a new function
with the old name and no `spp.cfi` (2026-11-08: 56 of 148 lost, 81 std tests trapping on their first indirect call). So
`ApplyCfiTypes` turns `spp.cfi` into LLVM's `!kcfi_type` **before** `RunOptimizationPipeline`; LLVM's passes keep that
one (checked with `opt-23 -passes=mergefunc`). `CheckCfiCalls`, the missing-bundle Ice, stays after optimisation.

**Why:** a missing bundle is an `Ice` in `CheckCfiCalls`, so any new indirect-call site must use `CfiCallBundles`; and
any typing read from s++ metadata must happen before the optimiser.
**How to apply:** a new closure-like value or call path: stamp the function (`StampCfiFn`) and bundle the call. A new
hardening step that reads custom function metadata runs before the optimiser too, or uses an LLVM-known kind.
Related: [[spp-single-abort-path]], [[hardening-needs-feature-gate]].
