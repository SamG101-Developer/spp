---
name: llvm-passes-plain-tu
description: "Only llvm_pipeline.hpp/.cpp is a plain TU (PassBuilder.h exposes a TU-local entity); the rest of the old llvm_passes is modules since 2026-10-19"
---

`llvm_passes.hpp/.cpp` was split on 2026-10-19 into six parts. Only `llvm_pipeline.hpp/.cpp` (coroutine lowering,
optimisation, internalize, LTO link) stays a plain TU that `#include`s LLVM: `LoopPassManager.h`, reached through
`PassBuilder.h`, declares `HasRunOnLoopT` in an anonymous namespace and `llvm::is_detected` exposes it - a hard error
in GCC (not the suppressible `-Wexpose-global-module-tu-local`). The maintainer's fix is llvm/llvm-project#233326 (open). The rest
are modules that `import llvm`: `spp.codegen.llvm_target` (target, triples, folders, target machine, object emission;
exports `TargetTriple()`), `llvm_entry` (C main, runtime shims), `llvm_harden_memory` (exports `kNoSplitStack`),
`llvm_harden_codegen` (exports `kFfiFnAttr`/`kCfiMd`/`kCfiCoroMd`), `llvm_artefact` (hardening record, fortification).

The plain TU still imports spp modules (`spp.utils.types`), so it uses `Str`/`Vec`/`Opt` and the banned-idiom alias
rules apply to it; `llvm_pipeline.hpp` is the allowlisted header (included into GMFs).

**How to apply:** new LLVM code goes in a module and gets what it needs exported from `headers/wrappers/llvm.ixx`,
including free operators (`operator==`/`+` on StringRef/Twine are exported there; ADL needs them). Only code that
builds a pass pipeline goes in `llvm_pipeline`. When #233326 lands, even that can become a module.
See [[llvm-module-wrapper-exports]].
