#pragma once

namespace llvm {
  class DominatorTree;
  class Function;
  class Instruction;
  class Loop;
  class LoopInfo;
  class Module;
  class StringRef;
  class User;
  class BasicBlock;
  template <typename Fn>
  class function_ref;
  template <typename IRUnitT, typename... ExtraArgTs>
  class AnalysisManager;
  template <typename IRUnitT, typename AnalysisManagerT, typename... ExtraArgTs>
  class PassManager;
}

/// The optimisation pipeline and the passes s++ adds to it, declared in one
/// place for all the different passes, but with split implementations found in
/// "sources/spp/codegen/llvm_passes/llvm_pass_xxx.cpp" (different subgroups).
///
/// Once LLVM24 is ready, https://github.com/llvm/llvm-project/pull/233326 will
/// be merged, and we can use a normal ".ixx" module, and "import llvm".
namespace spp::codegen {
  using FunctionPassManager = llvm::PassManager<
    llvm::Function, llvm::AnalysisManager<llvm::Function>>;

  using ModulePassManager = llvm::PassManager<
    llvm::Module, llvm::AnalysisManager<llvm::Module>>;

  /// The core entry into all the passes. This runs the normal analysis managers
  /// and the correct order, and handles the custom passes too. All coroutine
  /// lowering and checks are done here, on the lto module. Inject the target
  /// CPU (if available), and the optimisation level too.
  auto RunOptimizationPipeline(
    void *llvm_module, void *target_machine, unsigned opt_level,
    void (*after_coro_split)(void *llvm_module), bool air_integers) -> void;

  /// Copy "src_module" into "dest_module" (both opaque "llvm::Module" pointers,
  /// sharing one context), so the optimiser can see across what were separate
  /// modules. The "dest_module" is always the LTO module, and each individual
  /// source module is copied into it. The pipeline is ran over the LTO module.
  auto LinkIntoLtoModule(
    void *dest_module, void *src_module) -> bool;

  /// Make every symbol inside the LTO module "internal", except for the entry
  /// point, as nothing else needs to be exposed. All symbols were external
  /// prior to LTO, for cross-usage, but can now be internal. Declarations are
  /// left alone, such as "!ffi" functions, which link to the shared objects.
  auto RunInternalizePass(
    void *llvm_module, char const *const *preserved_names,
    unsigned long preserved_count) -> void;

  // ==== Cold paths ("llvm_pass_cold_path.cpp") ====

  /// The bodiless declaration a call to "std::hints::cold_path" is lowered to.
  /// "ApplyColdPathHints" removes every call to it, so the name never reaches
  /// an object file.
  inline constexpr auto kColdPathMarker = "spp.hint.cold_path";

  /// Turn the "cold_path" markers in a module into branch weights, then remove
  /// them. A block is cold if it holds a marker, or if every way out of it
  /// other than unwinding is cold. Runs on every module, at every optimisation
  /// level, before anything like coroutine splitting, inlining moves code out
  /// of the block it was emitted into.
  auto ApplyColdPathHints(void *llvm_module) -> void;

  // ==== AIR integers ("llvm_pass_air.cpp") ====

  /// Add the AIR passes to "fpm", a function pipeline run at the vectoriser's
  /// start, once inlining and the loop passes have left loops as simple as they
  /// will be: checked running sums computed exactly, and the overflow checks of
  /// a loop that does nothing observable moved to its exit. Both act only where
  /// vectors are 256 bits wide. Every overflow that matters still aborts, with
  /// its check's kind, before anything sees it.
  auto AddAirPasses(FunctionPassManager &fpm) -> void;

  /// Add the pass that puts each checked op back together for the backend to
  /// "mpm", run at the very end of the optimiser. A checked op's result is
  /// emitted as the plain op flagged as not wrapping, so the optimiser can read
  /// it; by the end it and its check have often drifted apart, and the backend
  /// only fuses them ("add", "jo") as one "*.with.overflow" intrinsic.
  auto AddCheckedFusePasses(ModulePassManager &mpm) -> void;

  // ==== Index checks hoisted ("llvm_pass_bounds.cpp") ====

  /// Add the pass that versions loops by their index checks to "fpm", run at
  /// the vectoriser's start, before the AIR passes. An index check in a loop is
  /// an exit that cannot be deferred (the read must not happen), so it keeps
  /// the loop scalar and branchy. A loop whose checks are an induction variable
  /// against a length that does not change in it gets one test before it: if no
  /// check can fail on any round the loop will run, it runs a copy without
  /// them; otherwise the original, which aborts exactly where it did.
  auto AddBoundsPasses(FunctionPassManager &fpm) -> void;

  // ==== Loop versioning ("llvm_pass_loop_versioning.cpp") ====

  /// A block that only aborts: one call to a function that does not return,
  /// then "unreachable". Every S++ runtime check fails into one.
  auto IsAbortBlock(llvm::BasicBlock const &bb) -> bool;

  /// Create a fast version, and original version of the loop. If we can
  /// determine how many times the loop will run (at runtime), then we can
  /// vectorise/unroll the loop to reduce the number of comparisons.
  auto VersionLoop(
    llvm::Loop &loop, llvm::BasicBlock *exit,
    llvm::function_ref<llvm::User*(llvm::Instruction *before)> make_condition,
    llvm::StringRef suffix, llvm::LoopInfo &li,
    llvm::DominatorTree &dt) -> void;
}
