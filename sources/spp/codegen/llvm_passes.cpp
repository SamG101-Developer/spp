#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Coroutines/CoroAnnotationElide.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <spp/codegen/llvm_passes.hpp>

namespace {
  /**
   * The one target machine every module is built against, created on first use. Registering the native target is done
   * here rather than at start-up so that nothing has to remember to do it before the first module is made.
   */
  auto HostTargetMachine()
    -> llvm::TargetMachine* {
    static auto *machine = []() -> llvm::TargetMachine* {
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();

      auto error = std::string();
      const auto triple = llvm::Triple(spp::codegen::kTargetTriple);
      const auto *target = llvm::TargetRegistry::lookupTarget(triple, error);
      if (target == nullptr) {
        llvm::errs() << "No llvm target for " << spp::codegen::kTargetTriple << ": " << error << "\n";
        return nullptr;
      }

      // "Position independent" because the runtime the program
      // links against is a shared library, and a non-pic object
      // cannot be linked against one on this target.
      return target->createTargetMachine(
        triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_);
    }();
    return machine;
  }
}

auto spp::codegen::HostDataLayoutString()
  -> char const* {
  static const auto layout = [] {
    const auto *machine = HostTargetMachine();
    return machine != nullptr ? machine->createDataLayout().getStringRepresentation() : std::string();
  }();
  return layout.c_str();
}

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

auto spp::codegen::RunOptimizationPipeline(
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

  // The per-module pipeline, which is also what the combined
  // module gets: once the modules are linked there is only one
  // of them, and the ordinary pipeline is what sees across the
  // file boundaries that used to separate them.
  auto module_pm = pass_builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
  module_pm.run(llvm_mod, module_am);
}

auto spp::codegen::LinkIntoLtoModule(
  void *dest_module,
  void *src_module)
  -> bool {
  auto &dest = *static_cast<llvm::Module*>(dest_module);
  auto clone = llvm::CloneModule(*static_cast<llvm::Module*>(src_module));
  return not llvm::Linker::linkModules(dest, std::move(clone));
}

auto spp::codegen::RunInternalizePass(
  void *llvm_module,
  char const *const *preserved_names,
  const unsigned long preserved_count)
  -> void {
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

auto spp::codegen::EmitCEntryPoint(
  void *llvm_module,
  char const *spp_main_name)
  -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();

  // Failsafe "main" check for missing "main", but S++ semantic
  // analysis should have picked this up earlier.
  const auto spp_main = llvm_mod.getFunction(spp_main_name);
  if (spp_main == nullptr) {
    llvm::errs() << "No entry point named " << spp_main_name << " in the module\n";
    return false;
  }

  // Failsafe "main" check for duplicate "main", but S++ semantic
  // analysis should have picked this up earlier.
  if (llvm_mod.getFunction("main") != nullptr) {
    llvm::errs() << "The module already declares a C entry point\n";
    return false;
  }

  const auto i32_ty = llvm::Type::getInt32Ty(ctx);
  const auto ptr_ty = llvm::PointerType::get(ctx, 0);
  const auto main_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
  const auto main_fn = llvm::Function::Create(
    main_ty, llvm::Function::ExternalLinkage, "main", &llvm_mod);

  auto builder = llvm::IRBuilder<>(llvm::BasicBlock::Create(ctx, "entry", main_fn));

  // Failsafe "main" check for 0-arg "main", but S++ semantic
  // analysis should have picked this up earlier.
  if (spp_main->arg_size() != 0) {
    llvm::errs() << "The entry point " << spp_main_name << " takes arguments; it must take none\n";
    return false;
  }

  const auto result = builder.CreateCall(spp_main, {});

  // An S++ "main" returns "Void", which is where this ends up;
  // the sppc::exit class of functions can be used to actually
  // exit with a non-0 code.
  builder.CreateRet(llvm::ConstantInt::get(i32_ty, 0));
  return true;
}

auto spp::codegen::ScrubCorruptLifetimeIntrinsics(
  void *llvm_module)
  -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  // Bin off a load of corrupted things.
  // Todo: Extremely hazardous band-aid, as I don't actually
  //  know that they do.
  auto broken = llvm::SmallVector<llvm::Function*>();
  for (auto &fn : llvm_mod) {
    if (not fn.isDeclaration() or not fn.getName().starts_with("llvm.")) { continue; }
    if (not fn.getReturnType()->isVoidTy()) { continue; }
    const auto unrecognised = fn.getIntrinsicID() == llvm::Intrinsic::not_intrinsic;
    if (unrecognised or fn.getName().starts_with("llvm.lifetime.")) { broken.push_back(&fn); }
  }

  auto scrubbed = 0UL;
  for (auto *fn : broken) {
    // Only dropped when every use is a plain call to it. Anything else
    // means this is not the shape being worked around, and it is left
    // alone to fail visibly rather than be quietly changed.
    auto calls = llvm::SmallVector<llvm::CallBase*>();
    auto only_calls = true;
    for (auto *user : fn->users()) {
      const auto call = llvm::dyn_cast<llvm::CallBase>(user);
      if (call != nullptr and call->getCalledFunction() == fn) { calls.push_back(call); }
      else { only_calls = false; }
    }
    if (not only_calls) { continue; }

    for (auto *call : calls) { call->eraseFromParent(); }
    fn->eraseFromParent();
    scrubbed += calls.size();
  }
  return scrubbed;
}

auto spp::codegen::EmitObjectFile(
  void *llvm_module,
  char const *path)
  -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto *machine = HostTargetMachine();
  if (machine == nullptr) { return false; }

  // Check we can open the given path.
  auto ec = std::error_code();
  auto out = llvm::raw_fd_ostream(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open " << path << ": " << ec.message() << "\n";
    return false;
  }

  // The legacy pass manager, because that is the only one
  // the code generator is driven through.
  auto codegen_pm = llvm::legacy::PassManager();
  if (machine->addPassesToEmitFile(codegen_pm, out, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    llvm::errs() << "The target cannot emit an object file\n";
    return false;
  }

  codegen_pm.run(llvm_mod);
  out.flush();
  return true;
}
