#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/ProfDataUtils.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Coroutines/CoroAnnotationElide.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_pipeline.hpp>

import spp.utils.ice;
import spp.utils.types;
import genex;

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

auto spp::codegen::ApplyColdPathHints(
  void *llvm_module) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  const auto marker = llvm_mod.getFunction(kColdPathMarker);
  if (marker == nullptr) { return; }

  // The weights "llvm.expect" gives a hinted branch, so
  // "unlikely(c)" and a "cold_path()" in its body weigh the
  // same.
  constexpr auto hot_weight = 2000u;
  constexpr auto cold_weight = 1u;

  // The functions holding a marker, each with the blocks its
  // markers sit in: the seed of that function's cold set.
  auto marked = Map<llvm::Function*, Set<llvm::BasicBlock*>>();
  auto calls = Vec<llvm::CallInst*>();
  for (const auto user : marker->users()) {
    const auto call = llvm::cast<llvm::CallInst>(user);
    marked[call->getFunction()].insert(call->getParent());
    calls.emplace_back(call);
  }

  // A successor the code is not expected to take: an unwind
  // edge, or a block ending in "unreachable" (such as the
  // dead fallthrough after an "else" arm, lowered as a branch
  // on "true"). Neither keeps a block hot.
  const auto never_taken = [](llvm::BasicBlock const *succ) {
    return succ->isEHPad() or llvm::isa<llvm::UnreachableInst>(succ->getTerminator());
  };

  // The successors a block continues through.
  const auto taken_succs = [&](llvm::BasicBlock const &bb) {
    auto succs = Vec<llvm::BasicBlock*>();
    const auto term = bb.getTerminator();
    for (auto i = 0u; i < term->getNumSuccessors(); ++i) {
      if (const auto succ = term->getSuccessor(i); not never_taken(succ)) {
        succs.emplace_back(succ);
      }
    }
    return succs;
  };

  for (auto &[fn, cold] : marked) {
    // Grow the cold set to a fixed point: a block with
    // somewhere to go, all of it cold, is cold. This reaches a
    // marker past a merge ("case a { case b {..} cold_path() }"),
    // which walking up from the marker would not.
    for (auto changed = true; changed;) {
      changed = false;
      for (auto &bb : *fn) {
        if (cold.contains(&bb)) { continue; }
        const auto succs = taken_succs(bb);
        if (succs.empty()) { continue; }
        if (genex::all_of(succs, [&](const auto succ) { return cold.contains(succ); })) {
          cold.insert(&bb);
          changed = true;
        }
      }
    }

    // Weight each branch or switch out of a hot block that
    // leads to a cold one, per successor. A branch out of a
    // cold block is left alone: the edge into that block
    // already carries the hint. A never-taken successor gets
    // the cold weight too, since weights given here replace
    // what llvm would otherwise have assumed about it.
    for (auto &bb : *fn) {
      if (cold.contains(&bb)) { continue; }
      const auto term = bb.getTerminator();
      if (not llvm::isa<llvm::CondBrInst>(term) and not llvm::isa<llvm::SwitchInst>(term)) { continue; }
      auto weights = llvm::SmallVector<std::uint32_t>();
      auto any_cold = false;
      for (auto i = 0u; i < term->getNumSuccessors(); ++i) {
        const auto succ = term->getSuccessor(i);
        const auto is_cold = cold.contains(succ);
        weights.emplace_back(is_cold or never_taken(succ) ? cold_weight : hot_weight);
        any_cold |= is_cold;
      }
      if (any_cold) { llvm::setBranchWeights(*term, weights, true); }
    }

    // Every path through the function is cold, so there is no
    // branch in it to weigh: the function itself is.
    if (cold.contains(&fn->getEntryBlock())) {
      fn->addFnAttr(llvm::Attribute::Cold);
    }
  }

  for (const auto call : calls) { call->eraseFromParent(); }
  marker->eraseFromParent();
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
  void *llvm_module, void *target_machine, const unsigned opt_level) -> void {
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
