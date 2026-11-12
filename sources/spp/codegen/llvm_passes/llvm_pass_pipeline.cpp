#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

import spp.utils.ice;
import spp.utils.types;
import genex;

namespace spp::codegen {
  namespace {
    /// A module pass running "hook" over the module, at the point
    /// of the pipeline it is added at. Only metadata changes, so
    /// every analysis survives it.
    struct AfterCoroSplitPass : llvm::PassInfoMixin<AfterCoroSplitPass> {
      void (*hook)(void *llvm_module);

      auto run(llvm::Module &llvm_mod, llvm::ModuleAnalysisManager &) -> llvm::PreservedAnalyses {
        hook(&llvm_mod);
        return llvm::PreservedAnalyses::all();
      }
    };
  }

  /// What a coroutine intrinsic's name starts with. This is
  /// used by the "PendingCoroUses" function to filter out
  /// coroutines from subroutines, when checking for uses.
  static constexpr auto kCoroIntrinsicPrefix = llvm::StringLiteral("llvm.coro.");

  /// The number of calls to a coroutine intrinsic still in
  /// the module: what "RunCoroLoweringPipeline" checks has
  /// reached zero once the coroutines are lowered.
  static auto PendingCoroUses(llvm::Module const &llvm_mod) -> unsigned long;

  /// Stop on any coroutine intrinsic the pipeline left behind.
  /// That is coroutine ir llvm could not lower, a fault in what
  /// codegen emitted: it would reach the backend as a call to an
  /// intrinsic nothing lowers, and every coroutine would trap on
  /// entry for want of a frame. The Ice names what was left.
  static auto CheckCoroutinesLowered(llvm::Module const &llvm_mod) -> void;
}

auto spp::codegen::PendingCoroUses(llvm::Module const &llvm_mod) -> unsigned long {
  // Get the declared functions (not just defined), filter
  // to get the coroutines (ignore subroutines), and then
  // get the "number of uses" of the function - this is the
  // number of "@llvm.coro.resume" calls which get lowered
  // away; check for 0.
  auto pending = 0UL;
  for (auto const &fn : llvm_mod) {
    if (not fn.isDeclaration() or not fn.getName().starts_with(kCoroIntrinsicPrefix)) { continue; }
    pending += fn.getNumUses();
  }
  return pending;
}

auto spp::codegen::CheckCoroutinesLowered(llvm::Module const &llvm_mod) -> void {
  const auto pending = PendingCoroUses(llvm_mod);
  if (pending == 0) { return; }
  auto left = Str();
  for (auto const &fn : llvm_mod) {
    if (not fn.isDeclaration() or not fn.getName().starts_with(kCoroIntrinsicPrefix) or fn.use_empty()) { continue; }
    left += (left.empty() ? "" : ", ") + fn.getName().str() + " (" + std::to_string(fn.getNumUses()) + ")";
  }
  spp::Ice(
    "coroutine lowering left " + std::to_string(pending) + " intrinsic call(s) in " + llvm_mod.getName().str() +
    ": " + left);
}

auto spp::codegen::RunOptimizationPipeline(
  void *llvm_module, void *target_machine, const unsigned opt_level,
  void (*after_coro_split)(void *llvm_module), const bool air_integers) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  auto loop_am = llvm::LoopAnalysisManager();
  auto func_am = llvm::FunctionAnalysisManager();
  auto cgscc_am = llvm::CGSCCAnalysisManager();
  auto module_am = llvm::ModuleAnalysisManager();

  // MergeFunctions folds identical bodies, mostly coroutine glue
  // and spawn trampolines that monomorphisation repeats per type
  // (-1.6% text on the std tests). O2 and up only.
  auto tuning = llvm::PipelineTuningOptions();
  tuning.MergeFunctions = opt_level >= 2;

  // The target machine is what supplies the cost model
  // ("TargetIRAnalysis") the vectorisers, unroller and inliner
  // read; the module's triple and data layout alone do not.
  auto pass_builder = llvm::PassBuilder(
    static_cast<llvm::TargetMachine*>(target_machine), tuning);
  pass_builder.registerModuleAnalyses(module_am);
  pass_builder.registerCGSCCAnalyses(cgscc_am);
  pass_builder.registerFunctionAnalyses(func_am);
  pass_builder.registerLoopAnalyses(loop_am);
  pass_builder.crossRegisterProxies(loop_am, func_am, cgscc_am, module_am);

  // Every coroutine is split by the time the optimiser proper
  // starts: "CoroSplit" runs with the inliner, so a coroutine is
  // inlined into its caller first, which is what lets "CoroElide"
  // put its frame on the caller's stack and call its resume
  // directly. The hook runs there, before MergeFunctions. O0's
  // pipeline has no such point, nor MergeFunctions: it runs last.
  if (after_coro_split != nullptr and opt_level > 0) {
    pass_builder.registerOptimizerEarlyEPCallback(
      [after_coro_split](llvm::ModulePassManager &mpm, llvm::OptimizationLevel, llvm::ThinOrFullLTOPhase) {
        mpm.addPass(AfterCoroSplitPass{.hook = after_coro_split});
      });
  }

  // An elided coroutine frame's suspend index is an "i2" field,
  // and SROA does not promote an integer narrower than the byte
  // it is stored in, so the index stays in memory and every
  // iteration of a loop over the coroutine stores and reloads it:
  // the loop never reduces to a counted one, nor vectorises
  // ("iter_mut", 2x C before). "mem2reg" promotes it. Peephole
  // points come after each InstCombine of the inliner's function
  // walk, which is where the frames are elided, and early enough
  // for the loop passes to see the promoted index.
  if (opt_level > 0) {
    pass_builder.registerPeepholeEPCallback(
      [](llvm::FunctionPassManager &fpm, llvm::OptimizationLevel) { fpm.addPass(llvm::PromotePass()); });
  }

  // The AIR passes ("AddAirPasses"): overflow checks moved to
  // where they cost least, just before the vectoriser.
  // Index checks proven before their loop, first: a loop left with one
  // exit is one the AIR passes can then work on.
  if (opt_level > 0) {
    pass_builder.registerVectorizerStartEPCallback(
      [](llvm::FunctionPassManager &fpm, llvm::OptimizationLevel) { AddBoundsPasses(fpm); });
  }
  if (air_integers and opt_level > 0) {
    pass_builder.registerVectorizerStartEPCallback(
      [](llvm::FunctionPassManager &fpm, llvm::OptimizationLevel) { AddAirPasses(fpm); });
  }

  // Checked ops and their checks put back together for the backend,
  // once the optimiser is done with the no-wrap flags.
  if (opt_level > 0) {
    pass_builder.registerOptimizerLastEPCallback(
      [](llvm::ModulePassManager &mpm, llvm::OptimizationLevel, llvm::ThinOrFullLTOPhase) { AddCheckedFusePasses(mpm); });
  }

  // The per-module pipeline, which is also what the combined
  // module gets: once the modules are linked there is only one
  // of them, and the ordinary pipeline is what sees across the
  // file boundaries that used to separate them. O0 has to go
  // through its own builder - the per-module one asserts on it.
  const auto level = [opt_level] {
    switch (opt_level) {
      case 0: return llvm::OptimizationLevel::O0;
      case 1: return llvm::OptimizationLevel::O1;
      case 2: return llvm::OptimizationLevel::O2;
      default: return llvm::OptimizationLevel::O3;
    }
  }();

  auto module_pm = opt_level == 0
    ? pass_builder.buildO0DefaultPipeline(level)
    : pass_builder.buildPerModuleDefaultPipeline(level);
  module_pm.run(llvm_mod, module_am);
  if (after_coro_split != nullptr and opt_level == 0) { after_coro_split(&llvm_mod); }
  CheckCoroutinesLowered(llvm_mod);
}

auto spp::codegen::LinkIntoLtoModule(
  void *dest_module, void *src_module) -> bool {
  auto &dest = *static_cast<llvm::Module*>(dest_module);
  auto clone = llvm::CloneModule(*static_cast<llvm::Module*>(src_module));
  return not llvm::Linker::linkModules(dest, std::move(clone));
}

auto spp::codegen::RunInternalizePass(
  void *llvm_module, char const *const *preserved_names,
  const unsigned long preserved_count) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  auto preserved = llvm::StringSet<>();
  for (auto i = 0UL; i < preserved_count; ++i) {
    preserved.insert(preserved_names[i]);
  }

  auto loop_am = llvm::LoopAnalysisManager();
  auto func_am = llvm::FunctionAnalysisManager();
  auto cgscc_am = llvm::CGSCCAnalysisManager();
  auto module_am = llvm::ModuleAnalysisManager();

  auto pass_builder = llvm::PassBuilder();
  pass_builder.registerModuleAnalyses(module_am);
  pass_builder.registerCGSCCAnalyses(cgscc_am);
  pass_builder.registerFunctionAnalyses(func_am);
  pass_builder.registerLoopAnalyses(loop_am);
  pass_builder.crossRegisterProxies(loop_am, func_am, cgscc_am, module_am);

  auto module_pm = llvm::ModulePassManager();
  module_pm.addPass(llvm::InternalizePass([&preserved](llvm::GlobalValue const &gv) {
    return preserved.contains(gv.getName());
  }));
  module_pm.addPass(llvm::GlobalDCEPass());
  module_pm.run(llvm_mod, module_am);
}
