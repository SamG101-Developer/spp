#include <cstdlib>
#include <cstring>
#include <iostream>

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
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

namespace {
  /** What a coroutine intrinsic's name starts with; see @c PendingCoroUses . */
  constexpr auto kCoroIntrinsicPrefix = llvm::StringLiteral("llvm.coro.");

  /**
   * How many calls to a coroutine intrinsic are still waiting to be lowered.
   *
   * @n
   * This is what the lowering pipeline is driving to zero, and so what says whether another run of it is worth
   * doing. The passes lower one intrinsic into another ("coro.resume" becomes "coro.subfn.addr"), so one run does
   * not finish the job; running until the count stops falling does, without assuming how many rounds that takes.
   *
   * @param llvm_mod The module to count in.
   * @return The number of uses of any @c "llvm.coro.*" declaration.
   */
  auto PendingCoroUses(
    llvm::Module const &llvm_mod)
    -> unsigned long {
    auto pending = 0UL;
    for (auto const &fn : llvm_mod) {
      if (not fn.isDeclaration() or not fn.getName().starts_with(kCoroIntrinsicPrefix)) { continue; }
      pending += fn.getNumUses();
    }
    return pending;
  }


  /** What every intrinsic name starts with, and the shortest a prefix can usefully be trimmed to. */
  constexpr auto kIntrinsicPrefix = llvm::StringLiteral("llvm.");

  /** How many repair-then-lower rounds the coroutine pipeline is allowed; see @c RunCoroLoweringPipeline . */
  constexpr auto kMaxCoroLoweringRounds = 4U;

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

    // General llvm preparation, get the context and some
    // types that need defining.
    auto &ctx = llvm_mod.getContext();
    const auto i32_ty = llvm::Type::getInt32Ty(ctx);

    // Get the functions that are needed specifically for
    // this shim: the "sppc_init" and "exit" functions.
    const auto init = llvm_mod.getOrInsertFunction(
      "sppc_init", llvm::FunctionType::get(i32_ty, {}, false));
    const auto exit_fn = llvm_mod.getOrInsertFunction(
      "exit", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i32_ty}, false));

    // Create the shim, a non-parameter, void-returning
    // function with internal linkage, which will be
    // called explicitly, and owns the setup calls.
    const auto shim = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}, false),
      llvm::Function::InternalLinkage, kRuntimeInitShim, &llvm_mod);

    // Standard building blocks setup for the shim
    // function, requiring the entry, "up" and failed
    // blocks.
    const auto entry_bb = llvm::BasicBlock::Create(ctx, "entry", shim);
    const auto up_bb = llvm::BasicBlock::Create(ctx, "rt.up", shim);
    const auto failed_bb = llvm::BasicBlock::Create(ctx, "rt.failed", shim);

    // Step 1: call the sppc_init boot function in the,
    // sppc C library, which itself calls a number of
    // boot functions. Check the result of that call,
    // and branch to "up" or "failed" depending on result.
    auto builder = llvm::IRBuilder<>(entry_bb);
    const auto init_rc = builder.CreateCall(init, {}, "rt.init");
    const auto init_ok = builder.CreateICmpEQ(init_rc, llvm::ConstantInt::get(i32_ty, 0), "rt.init.ok");
    builder.CreateCondBr(init_ok, up_bb, failed_bb);

    // Assuming init failed, the failed block calls "exit",
    // and marks the following zone as "unreachable" (ie if
    // "exit" cannot return).
    builder.SetInsertPoint(failed_bb);
    builder.CreateCall(exit_fn, {init_rc})->setDoesNotReturn();
    builder.CreateUnreachable();

    // The successful "up" block simply returns Void as
    // everything passed inside sppc.
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

    // General llvm preparation, get the context and some
    // types that need defining.
    auto &ctx = llvm_mod.getContext();

    // Get the functions that are needed specifically for
    // this shim: the "sppc_cleanup" function.
    const auto cleanup = llvm_mod.getOrInsertFunction(
      "sppc_cleanup", llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), {}, false));

    // Create the shim, a non-parameter, void-returning
    // function with internal linkage, which will be
    // called explicitly, and owns the cleanup calls.
    const auto shim = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}, false),
      llvm::Function::InternalLinkage, kRuntimeCleanupShim, &llvm_mod);

    // As this is the program teardown, there isn't really
    // anything a success vs failure can be measured with,
    // so call the entry block and be done. Possible error
    // from the "pthread_mutex_destroy" of a stdio lock
    // (EBUSY) but again not much we can do here.
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
    // Pull the triple from the llcm-known default, and
    // run it through normalization. Static because it's
    // always going to be the same.
    static const auto triple = llvm::Triple(
      llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
    return triple;
  }

  /**
   * Register the backends this build was linked against, once. The registry is what a triple is looked up in, so
   * nothing can be said about a requested target until this has run.
   */
  auto InitializeAllBackends()
    -> void {
    static const auto once = [] {
#ifdef SPP_ALL_TARGETS
      // All targets, including the host. This is used for
      // the CI pipeline cross-compilation checks, and for
      // normal cross-compilation.
      llvm::InitializeAllTargetInfos();
      llvm::InitializeAllTargets();
      llvm::InitializeAllTargetMCs();
      llvm::InitializeAllAsmPrinters();
#else
      // Native targets only (normal compilation, default to
      // the host).
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();
#endif
      return true;
    }();
    (void)once;
  }

  /**
   * The triple this build emits for, as chosen by @c SelectTarget . Empty means the host. Written once, before code
   * generation begins, and only read from there on - which is what makes the caching below safe.
   */
  auto SelectedTriple()
    -> std::string& {
    static auto triple = std::string();
    return triple;
  }

  /**
   * The target as a directory name.
   */
  auto SelectedFolderName()
    -> std::string& {
    static auto name = std::string(SPP_HOST_TRIPLE);
    return name;
  }

  /**
   * Whether @p text is usable as one path component: a triple's own alphabet, and nothing that would end the
   * component or walk out of it.
   */
  auto IsPathComponent(
    std::string const &text)
    -> bool {
    if (text.empty() or text == "." or text == "..") { return false; }
    for (const auto c : text) {
      const auto ok =
        (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9')
        or c == '-' or c == '_' or c == '.';
      if (not ok) { return false; }
    }
    return true;
  }

  /**
   * Canonical triples worth suggesting when a "--target" is not recognised. Not a whitelist, just examples.
   */
  constexpr char const *kSuggestedTriples[] = {
    "x86_64-unknown-linux-gnu",
    "x86_64-unknown-linux-musl",
    "i686-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "riscv64-unknown-linux-gnu",
    "s390x-unknown-linux-gnu",
    "powerpc64le-unknown-linux-gnu",
    "wasm32-unknown-wasi",
  };

  /**
   * Print the suggestions this build can actually emit for, one per line. Each is put through the same lookup a real
   * "--target" gets rather than mapped from a backend name, so what is offered is exactly what would be accepted.
   * @return How many were printed.
   */
  auto PrintSupportedTriples()
    -> unsigned {
    InitializeAllBackends();
    auto printed = 0u;
    for (auto const *candidate : kSuggestedTriples) {
      auto error = std::string();
      const auto triple = llvm::Triple(llvm::Triple::normalize(candidate));
      if (llvm::TargetRegistry::lookupTarget(triple, error) == nullptr) { continue; }
      llvm::errs() << "  " << candidate << "\n";
      printed += 1;
    }
    return printed;
  }

  /**
   * The triple every module is built for, normalised once: the selected target, or the host when none was asked for.
   */
  auto TargetTriple()
    -> llvm::Triple const& {
    static const auto triple = SelectedTriple().empty()
      ? HostTriple()
      : llvm::Triple(llvm::Triple::normalize(SelectedTriple()));
    return triple;
  }

  /**
   * The one target machine every module is built against, created on first use. Registering the backends is done here
   * rather than at start-up so that nothing has to remember to do it before the first module is made.
   */
  auto SelectedTargetMachine()
    -> llvm::TargetMachine* {
    static auto *machine = []() -> llvm::TargetMachine* {
      InitializeAllBackends();

      auto &registered = llvm::cl::getRegisteredOptions();
      if (const auto it = registered.find("disable-cgp"); it != registered.end()) {
        if (auto *const flag = static_cast<llvm::cl::opt<bool>*>(it->second)) { flag->setValue(true); }
      }

      auto error = std::string();
      const auto &triple = TargetTriple();
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

auto spp::codegen::SelectTarget(
  char const *triple)
  -> bool {
  // An empty request is the host, which is what the target
  // resolves to on its own when nothing has been selected.
  const auto requested = std::string(triple != nullptr ? triple : "");
  if (requested.empty()) { return true; }

  // The request names a directory, so it has to be one component
  // and nothing that leaves it. Checked before anything is done
  // with it rather than after.
  if (not IsPathComponent(requested)) {
    llvm::errs() << "Invalid target '" << requested << "': a triple is letters, digits, '-', '_' and '.'\n";
    return false;
  }

  // Normalised before the lookup, so a short name ("riscv64",
  // "aarch64") is accepted on the same terms as a full triple.
  InitializeAllBackends();
  const auto normalized = llvm::Triple::normalize(requested);

  auto error = std::string();
  if (llvm::TargetRegistry::lookupTarget(llvm::Triple(normalized), error) == nullptr) {
    llvm::errs()
      << "No llvm backend for target '" << requested << "' (normalised: " << normalized << "): " << error << "\n"
      << "This build can emit for:\n";

    // Triples rather than the registry's architecture names, which
    // is what the message used to print: "x86-64" is not something
    // that can be handed back to "--target", and a suggestion that
    // cannot be copied is not a suggestion.
    if (PrintSupportedTriples() == 0) {
      llvm::errs() << "  (nothing; this llvm has no usable backend at all)\n";
    }
    llvm::errs() << "Any other triple whose backend is linked in is accepted too; these are the tested ones.\n";
#ifndef SPP_ALL_TARGETS
    llvm::errs() << "Only the host backend is linked in; reconfigure with -DSPP_ALL_TARGETS=ON for the rest.\n";
#endif
    return false;
  }

  SelectedTriple() = normalized;

  // The folder keeps the requested spelling, not the normalised
  // one: normalize() returns a std::string across the boundary
  // described above, so what comes back cannot be trusted to be
  // a path component even though the request was.
  SelectedFolderName() = requested;
  return true;
}

auto spp::codegen::TargetIsHost()
  -> bool {
  return TargetTriple() == HostTriple();
}

auto spp::codegen::TargetFolderName()
  -> char const* {
  return SelectedFolderName().c_str();
}

auto spp::codegen::ApplyTargetToModule(
  void *llvm_module)
  -> void {
  // Both off the same machine, rather than off the selected triple
  // directly: a backend is allowed to answer with a triple other
  // than the one it was looked up by, and taking both from the
  // machine is what keeps a module's triple and layout describing
  // the same target.
  const auto *machine = SelectedTargetMachine();
  if (machine == nullptr) { return; }

  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  llvm_mod.setTargetTriple(machine->getTargetTriple());
  llvm_mod.setDataLayout(machine->createDataLayout());
}

auto spp::codegen::RunCoroLoweringPipeline(
  void *llvm_module)
  -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  // One run of the pipeline does not finish the job: the passes lower one intrinsic into another ("coro.resume"
  // becomes "coro.subfn.addr"), leaving the next pass something to do. So this runs until the coroutine intrinsics
  // stop disappearing, rather than a fixed number of times; the bound is there so a module that never reaches a
  // fixed point cannot spin.
  auto previous = PendingCoroUses(llvm_mod);
  for (auto round = 0U; round < kMaxCoroLoweringRounds; ++round) {

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

    const auto pending = PendingCoroUses(llvm_mod);
    if (pending == 0 or pending >= previous) { break; }
    previous = pending;
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


auto spp::codegen::ApplyStackProtector(
  void *llvm_module)
  -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  auto stamped = 0UL;
  for (auto &fn : llvm_mod) {
    // A declaration has no frame to protect, and a naked
    // function's prologue is whatever it says it is. Pre-
    // protected functions can be skipped here too.
    if (fn.isDeclaration() or fn.hasFnAttribute(llvm::Attribute::Naked)) { continue; }
    if (fn.hasFnAttribute(llvm::Attribute::StackProtectStrong)) { continue; }
    fn.addFnAttr(llvm::Attribute::StackProtectStrong);
    stamped += 1;
  }
  return stamped;
}

auto spp::codegen::AssertIntrinsicNamingIsSound()
  -> void {
  // Once per process, and not a walk of anything: this asks
  // llvm to mangle one intrinsic name and checks the answer,
  // because the failure being guarded against is not in any
  // particular module - it is that "Intrinsic::getName" itself
  // comes back wrong, which makes every overloaded intrinsic
  // in every module unnameable at once.
  [[maybe_unused]] static const auto checked = [] {
    auto ctx = llvm::LLVMContext();
    auto scratch = llvm::Module("spp.intrinsic.naming.check", ctx);
    auto *const i32 = llvm::Type::getInt32Ty(ctx);
    auto *const i1 = llvm::Type::getInt1Ty(ctx);
    auto *const fn_ty = llvm::FunctionType::get(llvm::StructType::get(ctx, {i32, i1}), {i32, i32}, false);

    const auto expected = llvm::StringRef("llvm.sadd.with.overflow.i32");
    const auto actual = llvm::Intrinsic::getName(llvm::Intrinsic::sadd_with_overflow, {i32}, &scratch, fn_ty);
    if (actual == expected) { return true; }

    llvm::errs()
      << "spp: llvm is mangling intrinsic names wrongly - got '" << actual << "', expected '" << expected << "'.\n"
      << "     Every overloaded intrinsic will now fail module verification. This is the libstdc++ '_M_create'\n"
      << "     incompatibility, which '-fvisibility-inlines-hidden' keeps this compiler clear of; see\n"
      << "     'docs/libstdcxx-m-create-abi-regression.md'.\n";
    return false;
  }();

  SPP_ASSERT(checked);
}

auto spp::codegen::EmitObjectFile(
  void *llvm_module,
  char const *path)
  -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto *machine = SelectedTargetMachine();
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
