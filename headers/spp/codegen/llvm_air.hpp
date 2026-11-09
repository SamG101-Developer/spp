#pragma once

/// AIR integers ("as-if infinitely ranged", .triages/air-integer-model.md):
/// checked arithmetic that stops before a wrong value is observed, rather
/// than at the operation, so its checks can be moved where they cost least.
/// A plain header, as "llvm_pipeline.hpp" is: the new pass manager's headers
/// expose a TU-local entity (llvm/llvm-project#233326), so the passes cannot
/// sit behind a module interface. Included by "llvm_pipeline.cpp" alone.

#include <llvm/IR/PassManager.h>

namespace spp::codegen {
  /// Add the AIR passes to "fpm", a function pipeline run at the
  /// vectoriser's start, once inlining and the loop passes have left
  /// loops as simple as they will be: checked running sums computed
  /// exactly ("ExactSumsPass"), and the overflow checks of a loop that
  /// does nothing observable moved to its exit ("DeferOverflowChecksPass").
  /// Both act only where vectors are 256 bits wide. Every overflow that
  /// matters still aborts, with its check's kind, before anything sees it.
  auto AddAirPasses(llvm::FunctionPassManager &fpm) -> void;

  /// Add the pass that puts each checked op back together for the
  /// backend ("FuseCheckedOpsPass") to "mpm", run at the very end of
  /// the optimiser (O1 and up, whatever "air_integers" says). A checked
  /// op's result is emitted as the plain op flagged as not wrapping, so
  /// the optimiser can read it; by the end it and its check have often
  /// drifted apart, and the backend only fuses them ("add", "jo") as one
  /// "*.with.overflow" intrinsic.
  auto AddCheckedFusePasses(llvm::ModulePassManager &mpm) -> void;
}
