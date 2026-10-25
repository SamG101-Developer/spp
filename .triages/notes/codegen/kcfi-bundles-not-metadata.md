---
name: kcfi-bundles-not-metadata
description: "S9.1 CFI tags calls with kcfi operand bundles at emission; custom call metadata is dropped by the optimiser"
---

S9.1 (2026-10-18): indirect calls carry a `kcfi` operand bundle from where they are emitted (`CfiCallBundles` in
llvm_fn.cpp), never custom instruction metadata: SimplifyCFG sinking, GVN PRE and similar merges drop metadata kinds
LLVM does not know (30 of 364 calls lost an `!spp.cfi` tag). Function-level metadata (`spp.cfi`, `spp.cfi.coro`) does
survive, including into CoroSplit's `.resume/.destroy/.cleanup` clones, so `ApplyCfi` types functions from it.
A bundle on `llvm.coro.resume`/`destroy` is accepted by the verifier and kept by coro lowering.

**Why:** a missing bundle is an `Ice` in `ApplyCfi`, so any new indirect-call site must use `CfiCallBundles`.
**How to apply:** a new closure-like value or call path: stamp the function (`StampCfiFn`) and bundle the call.
Related: [[spp-single-abort-path]], [[hardening-needs-feature-gate]].
