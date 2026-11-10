---
name: llvm-passes-plain-tu
description: "The pipeline and s++'s passes are plain TUs (PassBuilder.h TU-local entity, fixed in LLVM 24 by #233326): one header llvm_passes.hpp, sources in codegen/llvm_passes/; never forward declare llvm::Value in it (GCC)"
updated: 2026-11-10
---

The optimisation pipeline and the passes s++ adds to it are plain TUs that `#include` LLVM, not modules: `LoopPassManager.h`, reached through
`PassBuilder.h`, declares `HasRunOnLoopT` in an anonymous namespace and `llvm::is_detected` exposes it - a hard error
in GCC (not the suppressible `-Wexpose-global-module-tu-local`). The maintainer's fix is llvm/llvm-project#233326 (merged). The rest
are modules that `import llvm`: `spp.codegen.llvm_target` (target, triples, folders, target machine, object emission;
exports `TargetTriple()`), `llvm_entry` (C main, runtime shims), `llvm_harden_memory` (exports `kNoSplitStack`),
`llvm_harden_codegen` (exports `kFfiFnAttr`/`kCfiMd`/`kCfiCoroMd`), `llvm_artefact` (hardening record, fortification).

**Layout (2026-11-10):** one header, `headers/spp/codegen/llvm_passes.hpp`, declares all of them; the definitions are
`sources/spp/codegen/llvm_passes/llvm_pass_*.cpp`, one each: `pipeline` (coroutine lowering, optimisation,
internalize, LTO link), `cold_path` (`ApplyColdPathHints`), `air` (AIR integers), `bounds` (index checks hoisted, O17)
and `loop_versioning` (shared by `air` and `bounds`). (History: one `llvm_passes.hpp/.cpp` was split into six on
2026-10-19, leaving `llvm_pipeline`; `llvm_air`, `llvm_bounds` and `llvm_loop_versioning` followed with headers of
their own, and on 2026-11-10 the four headers were merged back into this one.)

**The header includes no LLVM header**, so module units that `import llvm` (`compiler_boot.cpp`, `llvm_fn_impls.cpp`)
can include it in their global module fragment: the LLVM types are forward declared, the pass managers spelled out as
`PassManager<Function, AnalysisManager<Function>>` (an alias cannot be forward declared), and `VersionLoop`'s callback
takes the insertion point, not an `IRBuilder<>&` (its default template arguments cannot be). **Never forward declare
`llvm::Value` there:** GCC (trunk 20261018) then fails to read the `llvm` module in every unit including it ("failed to
read compiled module cluster ...: Bad file data", "failed to load pendings"); every other class tried
(`Module`, `Function`, `Instruction`, `Loop`, `LoopInfo`, `DominatorTree`, `BasicBlock`, `StringRef`, `User`) and the
template ones are fine, so `llvm::User` stands in where a value is returned. Found by bisecting the forward
declarations (2026-11-10).

The plain TUs still import spp modules (`spp.utils.types`), so they use `Str`/`Vec`/`Opt` and the banned-idiom alias
rules apply to them; `llvm_passes.hpp` is the allowlisted header (included into GMFs, it uses no spp module).

#233326 merged 2026-11-09, so it ships in LLVM 24 (not 23.x). It is the only blocker: a probe module (2026-11-10)
whose GMF includes every LLVM header the plain TUs use, re-exporting every `llvm::` name they reference, with a
function pass, a loop pass, both adaptors and a `PassBuilder` pipeline in its purview, fails on LLVM 23 headers with
that one `HasRunOnLoopT` error alone, and builds cleanly (as does an importer adding its own loop pass) with the PR
applied, on GCC trunk 20261018. clang 23.1.5 accepts it even unpatched. MSVC and LLVM 24's own headers were not
tried.

**How to apply:** new LLVM code goes in a module and gets what it needs exported from `headers/wrappers/llvm.ixx`,
including free operators (`operator==`/`+` on StringRef/Twine are exported there; ADL needs them). Only code that
builds a pass pipeline or defines a pass goes in `llvm_passes/`. On the LLVM 24 bump they can all become modules; recheck any LLVM
header added to them since with the same kind of probe (upstream-watch U25).
See [[llvm-module-wrapper-exports]].
