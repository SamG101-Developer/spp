#include <cstring>

#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Coroutines/CoroAnnotationElide.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <spp/codegen/llvm_passes.hpp>

namespace {
  /** What every intrinsic name starts with, and the shortest a prefix can usefully be trimmed to. */
  constexpr auto kIntrinsicPrefix = llvm::StringLiteral("llvm.");

  /** How many repair-then-lower rounds the coroutine pipeline is allowed; see @c RunCoroLoweringPipeline . */
  constexpr auto kMaxCoroLoweringRounds = 4U;

  /** Intrinsics that only hint at what the optimizer may do; see @c RepairMisnamedIntrinsics . */
  constexpr auto kHintIntrinsicPrefix = llvm::StringLiteral("llvm.lifetime.");

  /** The width of the field an intrinsic name is read back out of, junk and all; see @c RepairMisnamedIntrinsics . */
  constexpr auto kNameFieldWidth = 31U;

  /**
   * Erase @p fn and every call to it. Only done when every use is a plain call: anything else means this is not the
   * shape being worked around, and it is left alone to fail visibly rather than be quietly changed.
   * @param[in,out] fn The declaration to drop.
   * @return @c true if it was dropped.
   */
  auto DropCallsTo(
    llvm::Function *fn)
    -> bool {
    auto calls = llvm::SmallVector<llvm::CallBase*>();
    for (auto *user : fn->users()) {
      const auto call = llvm::dyn_cast<llvm::CallBase>(user);
      if (call == nullptr or call->getCalledFunction() != fn) { return false; }
      calls.push_back(call);
    }

    for (auto *call : calls) { call->eraseFromParent(); }
    fn->eraseFromParent();
    return true;
  }

  /** The name the runtime start-up shim is emitted under; dotted, so it cannot collide with a mangled s++ name. */
  constexpr auto kRuntimeInitShim = llvm::StringLiteral("spp.rt.init");

  /** The name the runtime tear-down shim is emitted under; dotted, for the same reason. */
  constexpr auto kRuntimeCleanupShim = llvm::StringLiteral("spp.rt.cleanup");

  /**
   * Emit, once per module, an internal @c void(void) that brings the ffi runtime up and does not come back if it
   * cannot. @c sppc_init installs the signal dispositions, the locale and the malloc tuning that everything after it
   * assumes, starts the green-thread runtime, and builds the three stdio mutexes; a failure leaves those half-built.
   * @param[in,out] llvm_mod The module to emit it into.
   * @return The shim, existing or new.
   */
  auto RuntimeInitShim(
    llvm::Module &llvm_mod)
    -> llvm::Function* {
    if (auto *const existing = llvm_mod.getFunction(kRuntimeInitShim)) { return existing; }

    auto &ctx = llvm_mod.getContext();
    const auto i32_ty = llvm::Type::getInt32Ty(ctx);
    const auto init = llvm_mod.getOrInsertFunction(
      "sppc_init", llvm::FunctionType::get(i32_ty, {}, false));
    const auto exit_fn = llvm_mod.getOrInsertFunction(
      "exit", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i32_ty}, false));

    const auto shim = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}, false),
      llvm::Function::InternalLinkage, kRuntimeInitShim, &llvm_mod);

    const auto entry_bb = llvm::BasicBlock::Create(ctx, "entry", shim);
    const auto up_bb = llvm::BasicBlock::Create(ctx, "rt.up", shim);
    const auto failed_bb = llvm::BasicBlock::Create(ctx, "rt.failed", shim);

    auto builder = llvm::IRBuilder<>(entry_bb);
    const auto init_rc = builder.CreateCall(init, {}, "rt.init");
    const auto init_ok = builder.CreateICmpEQ(init_rc, llvm::ConstantInt::get(i32_ty, 0), "rt.init.ok");
    builder.CreateCondBr(init_ok, up_bb, failed_bb);

    // Marked here rather than left to the pipeline to infer, because the
    // "unreachable" after it is only well-formed if "exit" cannot return.
    builder.SetInsertPoint(failed_bb);
    builder.CreateCall(exit_fn, {init_rc})->setDoesNotReturn();
    builder.CreateUnreachable();

    builder.SetInsertPoint(up_bb);
    builder.CreateRetVoid();
    return shim;
  }

  /**
   * Emit, once per module, an internal @c void(void) wrapper around @c sppc_cleanup for @c atexit to be handed. The
   * runtime function returns an @c int , and registering it directly would have libc call it through a signature it
   * does not have; the wrapper drops the result instead.
   * @param[in,out] llvm_mod The module to emit it into.
   * @return The shim, existing or new.
   */
  auto RuntimeCleanupShim(
    llvm::Module &llvm_mod)
    -> llvm::Function* {
    if (auto *const existing = llvm_mod.getFunction(kRuntimeCleanupShim)) { return existing; }

    auto &ctx = llvm_mod.getContext();
    const auto cleanup = llvm_mod.getOrInsertFunction(
      "sppc_cleanup", llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), {}, false));

    const auto shim = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}, false),
      llvm::Function::InternalLinkage, kRuntimeCleanupShim, &llvm_mod);

    // The result is the errno a "pthread_mutex_destroy" of a stdio lock failed with: EBUSY means the program still
    // held one as it exited, which is a bug in it, but this runs too late for anything to be told about it.
    auto builder = llvm::IRBuilder<>(llvm::BasicBlock::Create(ctx, "entry", shim));
    builder.CreateCall(cleanup, {});
    builder.CreateRetVoid();
    return shim;
  }

  /**
   * The triple the host llvm was configured for, normalised once. Read from llvm rather than written down, so an
   * arm64 or a windows build asks its own backend for a layout and a code generator instead of x86's.
   */
  auto HostTriple()
    -> llvm::Triple const& {
    static const auto triple = llvm::Triple(llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
    return triple;
  }

  /**
   * The one target machine every module is built against, created on first use. Registering the native target is done
   * here rather than at start-up so that nothing has to remember to do it before the first module is made.
   */
  auto HostTargetMachine()
    -> llvm::TargetMachine* {
    static auto *machine = []() -> llvm::TargetMachine* {
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();

      auto &registered = llvm::cl::getRegisteredOptions();
      if (const auto it = registered.find("disable-cgp"); it != registered.end()) {
        if (auto *const flag = static_cast<llvm::cl::opt<bool>*>(it->second)) { flag->setValue(true); }
      }

      auto error = std::string();
      const auto &triple = HostTriple();
      const auto *target = llvm::TargetRegistry::lookupTarget(triple, error);
      if (target == nullptr) {
        llvm::errs() << "No llvm target for " << triple.str() << ": " << error << "\n";
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

auto spp::codegen::HostTargetTripleString()
  -> char const* {
  // Off the machine rather than off "HostTriple" directly: a backend is
  // allowed to answer with a triple other than the one it was looked up
  // by, and the layout below comes from the same machine, so taking both
  // from it is what keeps a module's triple and layout describing the
  // same target.
  static const auto triple = [] {
    const auto *machine = HostTargetMachine();
    return machine != nullptr ? machine->getTargetTriple().str() : std::string();
  }();
  return triple.c_str();
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

  // See "RepairMisnamedIntrinsics". A misnamed intrinsic is invisible to
  // these passes, so the names are put back before each run - and it takes
  // more than one run, because the passes lower one intrinsic into another
  // ("coro.resume" becomes "coro.subfn.addr") and the replacement is
  // misnamed in its turn, leaving the next pass nothing to work on. Settles
  // in three rounds; the bound is there so a repair that never reaches a
  // fixed point cannot spin.
  for (auto round = 0U; round < kMaxCoroLoweringRounds; ++round) {
    if (RepairMisnamedIntrinsics(&llvm_mod) == 0 and round > 0) { break; }

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
}

auto spp::codegen::RunOptimizationPipeline(
  void *llvm_module,
  const unsigned opt_level)
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

  const auto atexit_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, false);
  builder.CreateCall(RuntimeInitShim(llvm_mod), {});
  builder.CreateCall(llvm_mod.getOrInsertFunction("atexit", atexit_ty), {RuntimeCleanupShim(llvm_mod)});
  builder.CreateCall(spp_main, {});

  // An S++ "main" returns "Void", which is where this ends up;
  // the sppc::exit class of functions can be used to actually
  // exit with a non-0 code.
  builder.CreateRet(llvm::ConstantInt::get(i32_ty, 0));
  return true;
}

auto spp::codegen::RepairMisnamedIntrinsics(
  void *llvm_module)
  -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  // Collected first, because the repair adds to and removes from
  // the function list that this is walking.
  auto broken = llvm::SmallVector<llvm::Function*>();
  for (auto &fn : llvm_mod) {
    if (not fn.isDeclaration() or not fn.getName().starts_with("llvm.")) { continue; }
    const auto id = fn.getIntrinsicID();
    if (id == llvm::Intrinsic::not_intrinsic or llvm::Intrinsic::isOverloaded(id)) { broken.push_back(&fn); }
  }

  auto repaired = 0UL;
  for (auto *fn : broken) {
    // The longest prefix llvm resolves is the real name. Going longest-first
    // keeps the overload suffix on the mangled ones ("llvm.lifetime.start.p0"
    // resolves, and so would "llvm.lifetime.start" on its own); going one
    // character at a time is what stops printable garbage being taken for
    // part of the name, which a scan for the first unprintable byte would do.
    const auto name = fn->getName();
    auto real_name = llvm::StringRef();
    auto rebuilt = std::string();

    // Bug (?) where all names are a fixed length, presenting
    // as corrupt strings because the genuine name doesn't
    // necessarily reach the 31-byte limit. Patch this by byte-
    // stripping.
    auto base = llvm::StringRef();
    if (const auto nul = name.find('\0'); nul != llvm::StringRef::npos and nul < kNameFieldWidth) {
      base = llvm::StringRef(name.data(), nul);
      if (llvm::Intrinsic::lookupIntrinsicID(base) == llvm::Intrinsic::not_intrinsic) { base = llvm::StringRef(); }
    }

    for (auto len = name.size(); base.empty() and len > kIntrinsicPrefix.size(); --len) {
      const auto candidate = name.substr(0, len);
      if (llvm::Intrinsic::lookupIntrinsicID(candidate) == llvm::Intrinsic::not_intrinsic) { continue; }
      base = candidate;
      break;
    }
    if (base.empty()) { continue; }

    // Hints are dropped because of corruption. Todo: Can we now
    // re-enable them now we have the byte-stripping in place for
    // corrupt names?
    if (base.starts_with(kHintIntrinsicPrefix)) {
      if (DropCallsTo(fn)) { repaired += 1; }
      continue;
    }

    rebuilt.assign(base.data(), base.size());
    const auto id = llvm::Intrinsic::lookupIntrinsicID(base);

    if (llvm::Intrinsic::isOverloaded(id)) {
      auto tys = llvm::SmallVector<llvm::Type*>();
      if (not llvm::Intrinsic::isSignatureValid(id, fn->getFunctionType(), tys)) { continue; }

      const auto mangled = llvm::Intrinsic::getName(id, tys, &llvm_mod, fn->getFunctionType());
      const auto *const raw = mangled.c_str();
      if (std::strlen(raw) >= kNameFieldWidth) { continue; }

      const auto *const suffix = raw + kNameFieldWidth;
      const auto suffix_len = std::strlen(suffix);
      if (suffix_len == 0) { continue; }

      // A name that already ends in the suffix is a name that
      // was already right - either it was never damaged, or an
      // earlier round repaired it.
      const auto tail = base.size() >= suffix_len
        ? llvm::StringRef(base.data() + base.size() - suffix_len, suffix_len)
        : llvm::StringRef();
      if (tail == llvm::StringRef(suffix, suffix_len)) { continue; }
      rebuilt.append(suffix, suffix_len);
    }

    real_name = llvm::StringRef(rebuilt.c_str(), std::strlen(rebuilt.c_str()));

    // A declaration under the real name may already be here, from a call
    // site whose name survived. Reusing it is the point - two declarations
    // of one intrinsic would leave the second renamed and unrecognised
    // again. Only ever reused when the types agree; a mismatch is not the
    // shape being worked around, so it is left to fail visibly.
    auto *fixed = llvm_mod.getFunction(real_name);
    if (fixed == fn) { continue; }
    if (fixed != nullptr and fixed->getFunctionType() != fn->getFunctionType()) { continue; }
    if (fixed == nullptr) {
      fixed = llvm::Function::Create(
        fn->getFunctionType(), llvm::GlobalValue::ExternalLinkage, real_name, &llvm_mod);
      fixed->copyAttributesFrom(fn);
    }

    // Nothing is gained by a rename that llvm reads back as broken as what
    // it replaced, and the old declaration is worth keeping in that case so
    // the failure is still visible downstream.
    if (fixed->getIntrinsicID() == llvm::Intrinsic::not_intrinsic) {
      if (fixed != fn and fixed->use_empty()) { fixed->eraseFromParent(); }
      continue;
    }

    fn->replaceAllUsesWith(fixed);
    fn->eraseFromParent();
    repaired += 1;
  }
  return repaired;
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
