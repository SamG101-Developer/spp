#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Coroutines/CoroAnnotationElide.h>

#include <spp/codegen/llvm_passes.hpp>

auto spp::codegen::RunCoroLoweringPipeline(
  void *llvm_module)
  -> void {
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

  // O0 - "the minimal semantically required passes". Coroutine
  // lowering is a correctness requirement.
  auto module_pm = pass_builder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
  module_pm.addPass(llvm::createModuleToPostOrderCGSCCPassAdaptor(llvm::CoroAnnotationElidePass()));
  module_pm.run(llvm_mod, module_am);
}
