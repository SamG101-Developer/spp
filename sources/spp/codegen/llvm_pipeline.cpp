#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Coroutines/CoroAnnotationElide.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_pipeline.hpp>

import spp.utils.ice;
import spp.utils.types;

namespace spp::codegen {
  namespace {
    /// What a coroutine intrinsic's name starts with. This is
    /// used by the "PendingCoroUses" function to filter out
    /// coroutines from subroutines, when checking for uses.
    constexpr auto kCoroIntrinsicPrefix = llvm::StringLiteral("llvm.coro.");

    /// The number of calls to a coroutine intrinsic still in
    /// the module: what "RunCoroLoweringPipeline" checks has
    /// reached zero once the coroutines are lowered.
    auto PendingCoroUses(llvm::Module const &llvm_mod) -> unsigned long {
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
  }
}

auto spp::codegen::RunCoroLoweringPipeline(
  void *llvm_module) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  // One run lowers everything: the O0 pipeline holds the whole
  // chain, from "CoroEarly" (a resume or destroy becomes a
  // "coro.subfn.addr" and an indirect call) through "CoroSplit"
  // to "CoroCleanup" (which lowers what is left of that).
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

  // O0 - "the minimal semantically required passes". Coroutine
  // lowering is a correctness requirement.
  auto module_pm = pass_builder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
  module_pm.addPass(llvm::createModuleToPostOrderCGSCCPassAdaptor(llvm::CoroAnnotationElidePass()));
  module_pm.run(llvm_mod, module_am);

  // Anything left is coroutine ir llvm could not lower, which
  // is a fault in what codegen emitted: it would reach the
  // backend as a call to an intrinsic nothing lowers, and every
  // coroutine would trap on entry for want of a frame. Stop
  // here instead, naming what was left.
  if (const auto pending = PendingCoroUses(llvm_mod); pending != 0) {
    auto left = Str();
    for (auto const &fn : llvm_mod) {
      if (not fn.isDeclaration() or not fn.getName().starts_with(kCoroIntrinsicPrefix) or fn.use_empty()) { continue; }
      left += (left.empty() ? "" : ", ") + fn.getName().str() + " (" + std::to_string(fn.getNumUses()) + ")";
    }
    spp::Ice(
      "coroutine lowering left " + std::to_string(pending) + " intrinsic call(s) in " + llvm_mod.getName().str() +
      ": " + left);
  }
}

auto spp::codegen::RunOptimizationPipeline(
  void *llvm_module, const unsigned opt_level) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

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
  for (auto i = 0UL; i < preserved_count; ++i) { preserved.insert(preserved_names[i]); }

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
