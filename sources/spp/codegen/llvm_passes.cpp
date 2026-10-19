#include <cstdlib>
#include <cstring>
#include <iostream>

#include <llvm/ADT/StringSet.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Passes/PassBuilder.h>
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
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

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

    /// The name the runtime start-up shim is emitted under;
    /// dotted, so it cannot collide with a mangled s++ name.
    constexpr auto kRuntimeInitShim = llvm::StringLiteral("spp.rt.init");

    /// The name the runtime tear-down shim is emitted under;
    /// dotted, so it cannot collide with a mangled s++ name.
    constexpr auto kRuntimeCleanupShim = llvm::StringLiteral("spp.rt.cleanup");

    /// Marks a function that cannot have split stacks.
    constexpr auto kNoSplitStack = llvm::StringLiteral("spp-no-split-stack");

    /// Attributes for probing stacks.
    constexpr auto kProbeStackAttr = llvm::StringLiteral("probe-stack");
    constexpr auto kProbeStackInlineAsm = llvm::StringLiteral("inline-asm");

    /// The function attribute that builds a function's
    /// switches without jump tables.
    constexpr auto kNoJumpTablesAttr = llvm::StringLiteral("no-jump-tables");

    /// The function attribute that clamps a jump table's
    /// index on AArch64 before the table is read.
    constexpr auto kJumpTableHardeningAttr = llvm::StringLiteral("aarch64-jump-table-hardening");

    /// Emit, once per module, an internal "void(void)" that
    /// boots the sppc runtime, which calls the sppc "_sppc_init"
    /// function. This sets up the signal dispositions, locale,
    /// malloc tuning etc. It also boots the green thread async
    /// runtime, and initializes the stdio mutexes. Map the
    /// thread's unsafe stack too. Pass in flags for memory
    /// hardening too.
    auto RuntimeInitShim(
      llvm::Module &llvm_mod, const bool split_stacks,
      const bool deny_wx, const bool seal) -> llvm::Function* {
      // Guard against duplicating the init shim. This should
      // never trigger but is just a failsafe.
      if (const auto existing = llvm_mod.getFunction(kRuntimeInitShim)) { return existing; }

      // General llvm preparation, get the context and some
      // types that need defining.
      auto &ctx = llvm_mod.getContext();
      const auto i32_ty = llvm::Type::getInt32Ty(ctx);
      const auto step_ty = llvm::FunctionType::get(i32_ty, {}, false);

      // The start-up steps, in order. Each is an "int(void)"
      // runtime function answering 0 when it succeeded, and
      // anything else is the code the process exits with.
      auto steps = llvm::SmallVector<llvm::StringRef, 4>{"sppc_init"};
      if (split_stacks) { steps.emplace_back("sppc_unsafe_stack_up"); }
      if (deny_wx) { steps.emplace_back("sppc_deny_wx"); }
      if (seal) { steps.emplace_back("sppc_seal_segments"); }

      // Create the shim, a non-parameter, void-returning
      // function with internal linkage, which will be
      // called explicitly, and owns the setup calls.
      const auto shim = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}, false),
        llvm::Function::InternalLinkage, kRuntimeInitShim, &llvm_mod);

      // The function that maps the unsafe stack cannot be
      // a function that stands on one.
      shim->addFnAttr(kNoSplitStack);

      // The failed block is shared: it exits with whichever
      // step's code brought it there, and "exit" does not
      // return. Its phi is filled in as the steps are built.
      const auto entry_bb = llvm::BasicBlock::Create(ctx, "entry", shim);
      const auto failed_bb = llvm::BasicBlock::Create(ctx, "rt.failed", shim);
      auto builder = llvm::IRBuilder(failed_bb);
      const auto code = builder.CreatePHI(i32_ty, static_cast<unsigned>(steps.size()), "rt.failed.rc");
      const auto exit_fn = llvm_mod.getOrInsertFunction(
        "exit", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i32_ty}, false));
      builder.CreateCall(exit_fn, {code})->setDoesNotReturn();
      builder.CreateUnreachable();

      // Each step calls its runtime function and carries on
      // to the next only on 0. The last one carries on to
      // "up", which returns, as everything came up.
      builder.SetInsertPoint(entry_bb);
      for (auto const &step : steps) {
        const auto rc = builder.CreateCall(llvm_mod.getOrInsertFunction(step, step_ty), {}, "rt." + step);
        const auto next_bb = llvm::BasicBlock::Create(ctx, "rt." + step + ".ok", shim);
        builder.CreateCondBr(builder.CreateICmpEQ(rc, llvm::ConstantInt::get(i32_ty, 0)), next_bb, failed_bb);
        code->addIncoming(rc, builder.GetInsertBlock());
        builder.SetInsertPoint(next_bb);
      }
      builder.CreateRetVoid();
      return shim;
    }

    /// Emit, once per module, an internal "void(void)" that
    /// exits the sppc runtime, which calls the sppc "_sppc_cleanup"
    /// function. This cleans up the green thread runtime, and
    /// frees the stdio mutexes.
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
      auto builder = llvm::IRBuilder(llvm::BasicBlock::Create(ctx, "entry", shim));
      builder.CreateCall(cleanup, {});
      builder.CreateRetVoid();
      return shim;
    }

    /// The triple this build emits for, written into by
    /// "SelectTarget". If it is empty, then it's the host, as
    /// "TargetTriple" specifies. Only written once, before
    /// codegen begins.
    auto SelectedTriple() -> Str& {
      // Static string returned as a mutable reference for
      // writing into.
      static auto triple = Str();
      return triple;
    }

    /// The triple the host llvm was configured for, normalised
    /// once. Read from llvm so that an arm64 or windows build
    /// asks its own backend for a layout and code generator
    /// instead of x86's.
    auto HostTriple() -> llvm::Triple const& {
      // Pull the triple from the llvm-known default, and run it
      // through normalization. Static because it's always going
      // to be the same.
      static const auto triple = llvm::Triple(
        llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
      return triple;
    }

    /// The triple every module is built for, normalised once:
    /// the selected target, or the host when none was asked for.
    /// Being different from the host is a "cross compilation".
    auto TargetTriple() -> llvm::Triple const& {
      static const auto triple = SelectedTriple().empty()
        ? HostTriple()
        : llvm::Triple(llvm::Triple::normalize(SelectedTriple()));
      return triple;
    }

    /// Register the backends that this build was linked against,
    /// once. The registry is what a triple is looked up in, so
    /// nothing can be said about a requested target until this
    /// has run.
    auto InitializeAllBackends()
      -> void {
      SPP_ATTR_MAYBE_UNUSED static const auto once = [] {
      #if defined(SPP_ALL_TARGETS)
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
    }

    /// Canonical triples worth suggesting when a "--target" is
    /// not recognised. Not a whitelist, just examples.
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

    /// Print the suggestions that this build can actually emit
    /// for. Nothing that shows in "--targets" fails the lookup
    /// check.
    auto PrintSupportedTriples() -> unsigned {
      InitializeAllBackends();
      auto printed = 0u;
      for (const auto candidate : kSuggestedTriples) {
        auto error = Str();
        const auto triple = llvm::Triple(llvm::Triple::normalize(candidate));
        if (llvm::TargetRegistry::lookupTarget(triple, error) == nullptr) { continue; }
        llvm::errs() << "  " << candidate << "\n";
        printed += 1;
      }
      return printed;
    }

    /// Get the target machine from the target triple. Every
    /// module is built against this target machine. Done at
    /// startup.
    auto SelectedTargetMachine() -> llvm::TargetMachine* {
      static const auto machine = []() -> llvm::TargetMachine* {
        // Initialize all the backends for the configuration.
        // This makes target machines available.
        InitializeAllBackends();

        // Get the target triple (what we are compiling for),
        // and lookup the target for it.
        auto error = Str();
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

    /// Create the folder for the triple: "<arch>-<os>[-<env>]",
    /// built from the parsed enums.
    auto FolderOf(
      llvm::Triple const &triple) -> Str {
      auto folder = llvm::Triple::getArchTypeName(triple.getArch()).str();
      folder += "-";
      folder += triple.isMacOSX()
        ? "macos"
        : llvm::Triple::getOSTypeName(triple.getOS()).str();
      if (triple.getEnvironment() != llvm::Triple::UnknownEnvironment) {
        folder += "-";
        folder += llvm::Triple::getEnvironmentTypeName(triple.getEnvironment()).str();
      }
      return folder;
    }

    /// Convert a folder name into a llvm triple (optional because
    /// it could fail).
    auto TripleOfFolder(
      Str const &folder) -> Opt<llvm::Triple> {
      auto parts = llvm::SmallVector<llvm::StringRef, 4>();
      llvm::StringRef(folder).split(parts, '-');
      if (parts.size() != 2 and parts.size() != 3) { return Nullopt; }
      return llvm::Triple(llvm::Triple::normalize(folder));
    }

    /// Whether the selected target has straight-line speculation
    /// hardening: x86 and AArch64 do, nothing else does.
    auto TargetHasSlsHardening() -> bool {
      return TargetTriple().isX86() or TargetTriple().isAArch64();
    }

    /// Whether the selected target can scrub registers on return:
    /// x86 and AArch64 can.
    auto TargetHasRegisterScrubbing() -> bool {
      return TargetTriple().isX86() or TargetTriple().isAArch64();
    }

    /// Whether the selected target can check indirect calls by
    /// type (KCFI): x86-64 and AArch64 can.
    auto TargetHasKcfi() -> bool {
      return TargetTriple().getArch() == llvm::Triple::x86_64 or TargetTriple().isAArch64();
    }

    /// Where a program's hardening record lives: an ELF note,
    /// which "strip" keeps, or a section of its own elsewhere.
    constexpr auto kHardeningNoteSection = llvm::StringLiteral(".note.spp.hardening");
    constexpr auto kHardeningMachOSection = llvm::StringLiteral("__spp_hardening");
    constexpr auto kHardeningCoffSection = llvm::StringLiteral(".spphard");

    /// The owner the hardening record's ELF note names, and the
    /// note's type under that owner.
    constexpr auto kHardeningNoteOwner = llvm::StringLiteral("SPP");
    constexpr auto kHardeningNoteType = 1U;

    /// "n" rounded up to the four bytes every part of an ELF
    /// note is padded to.
    constexpr auto NotePadded(const std::size_t n) -> std::size_t {
      return (n + 3) & ~std::size_t{3};
    }

    /// The type operand "i" of a control-flow-integrity record
    /// holds.
    auto CfiTypeAt(llvm::MDNode const &md, const unsigned i) -> llvm::ConstantInt* {
      return llvm::mdconst::extract<llvm::ConstantInt>(md.getOperand(i));
    }
  }
}

auto spp::codegen::SelectTarget(
  char const *triple) -> bool {
  // An empty request is the host, which is what the target
  // resolves to on its own when nothing has been selected.
  const auto requested = Str(triple != nullptr ? triple : "");
  if (requested.empty()) { return true; }

  // Normalised before the lookup, so a short name ("riscv64",
  // "aarch64") is accepted on the same terms as a full triple.
  InitializeAllBackends();
  const auto normalized = llvm::Triple::normalize(requested);

  auto error = Str();
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
#if !defined(SPP_ALL_TARGETS)
    llvm::errs() << "Only the host backend is linked in; reconfigure with -DSPP_ALL_TARGETS=ON for the rest.\n";
#endif
    return false;
  }

  SelectedTriple() = normalized;
  return true;
}

/// [CHECKED]
auto spp::codegen::TargetIsHost() -> bool {
  // Compare the target and host triples; if they match then
  // the target is the host, and linking can proceed.
  return TargetTriple() == HostTriple();
}

/// [CHECKED]
auto spp::codegen::TargetFolderName() -> char const* {
  // The target folder name is the "folder of" the target triple,
  // ie the target that we are compiling to.
  static const auto folder = FolderOf(TargetTriple());
  return folder.c_str();
}

/// [CHECKED]
auto spp::codegen::HostFolderName() -> char const* {
  // The host folder name is the "folder of" the host triple, ie
  // the target that we are compiling to.
  static const auto folder = FolderOf(HostTriple());
  return folder.c_str();
}

/// [CHECKED]
auto spp::codegen::IsTargetFolderName(
  char const *folder) -> bool {
  const auto spelled = Str(folder);
  const auto triple = TripleOfFolder(spelled);
  if (not triple.has_value()) { return false; }
  if (triple->getArch() == llvm::Triple::UnknownArch) { return false; }
  if (triple->getOS() == llvm::Triple::UnknownOS) { return false; }
  return FolderOf(*triple) == spelled;
}

/// [CHECKED]
auto spp::codegen::SharedLibraryExtension(
  char const *folder) -> char const* {
  const auto triple = TripleOfFolder(folder);
  SPP_ASSERT(triple.has_value());
  if (triple->isOSWindows()) { return "dll"; }
  if (triple->isOSDarwin()) { return "dylib"; }
  return "so";
}

/// [CHECKED]
auto spp::codegen::ApplyTargetToModule(
  void *llvm_module) -> void {
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

  // The machine's PIC relocation model is not enough on its own:
  // a module with no PIC level counts as non-PIC, so an external
  // global (AArch64's "__stack_chk_guard") is read directly, which
  // a PIE link rejects. The PIE level keeps the module's own
  // symbols direct; COFF has no PIE, as with clang.
  llvm_mod.setPICLevel(llvm::PICLevel::BigPIC);
  if (not machine->getTargetTriple().isOSBinFormatCOFF()) {
    llvm_mod.setPIELevel(llvm::PIELevel::Large);
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

auto spp::codegen::EmitCEntryPoint(
  void *llvm_module, char const *spp_main_name, const bool split_stacks,
  const bool deny_wx, const bool seal) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();

  // Analysis has already checked "main" exists and is the only
  // one, so either failing here is a compiler bug.
  const auto spp_main = llvm_mod.getFunction(spp_main_name);
  if (spp_main == nullptr) { spp::Ice(Str("no entry point named ") + spp_main_name + " in the module"); }
  if (llvm_mod.getFunction("main") != nullptr) { spp::Ice("the module already declares a C entry point"); }

  const auto i32_ty = llvm::Type::getInt32Ty(ctx);
  const auto ptr_ty = llvm::PointerType::get(ctx, 0);
  const auto main_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
  const auto main_fn = llvm::Function::Create(
    main_ty, llvm::Function::ExternalLinkage, "main", &llvm_mod);

  // Nothing has mapped an unsafe stack at the point this function's
  // own prologue runs - the call that maps it is the first thing in
  // the body - so this frame stays whole.
  main_fn->addFnAttr(kNoSplitStack);

  auto builder = llvm::IRBuilder<>(llvm::BasicBlock::Create(ctx, "entry", main_fn));

  // Analysis has already checked "main" takes no arguments.
  if (spp_main->arg_size() != 0) {
    spp::Ice(Str("the entry point ") + spp_main_name + " takes arguments; it must take none");
  }

  // Todo: This might break performance.
  if (split_stacks) { spp_main->addFnAttr(llvm::Attribute::NoInline); }

  const auto atexit_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, false);
  builder.CreateCall(RuntimeInitShim(llvm_mod, split_stacks, deny_wx, seal), {});
  builder.CreateCall(llvm_mod.getOrInsertFunction("atexit", atexit_ty), {RuntimeCleanupShim(llvm_mod)});
  builder.CreateCall(spp_main, {});

  // An S++ "main" returns "Void", which is where this ends up;
  // the sppc::exit class of functions can be used to actually
  // exit with a non-0 code.
  builder.CreateRet(llvm::ConstantInt::get(i32_ty, 0));
}

auto spp::codegen::ApplyStackProtector(
  void *llvm_module) -> unsigned long {
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

auto spp::codegen::ApplySlsHardening(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  if (not TargetHasSlsHardening()) {
    llvm::errs()
      << "This target has no straight-line speculation hardening.\n"
      << "  To build without it, set 'sls = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // The features name the branches the trap follows: returns
  // and indirect jumps on x86, returns, indirect branches and
  // indirect calls on AArch64. Added to whatever features a
  // function already carries, as the backend reads them per
  // function rather than off the target machine.
  const auto features = TargetTriple().isX86()
    ? llvm::StringRef("+harden-sls-ret,+harden-sls-ijmp")
    : llvm::StringRef("+harden-sls-retbr,+harden-sls-blr");

  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration()) { continue; }
    const auto existing = fn.getFnAttribute("target-features").getValueAsString();
    if (existing.contains(features)) { continue; }
    fn.addFnAttr("target-features", existing.empty() ? features.str() : (existing + "," + features).str());
  }
  return true;
}

auto spp::codegen::ApplyRegisterScrubbing(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  if (not TargetHasRegisterScrubbing()) {
    llvm::errs()
      << "This target cannot scrub registers on return.\n"
      << "  To build without it, set 'scrub = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // "used-gpr": only the general-purpose registers the function
  // touched. Vector registers too ("all") would cost far more
  // on every return for little a pointer-sized leak needs.
  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration() or fn.hasFnAttribute(llvm::Attribute::Naked)) { continue; }
    if (fn.hasFnAttribute("zero-call-used-regs")) { continue; }
    fn.addFnAttr("zero-call-used-regs", "used-gpr");
  }
  return true;
}

auto spp::codegen::ApplyCfi(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  if (not TargetHasKcfi()) {
    llvm::errs()
      << "This target cannot check indirect calls by type.\n"
      << "  To build without it, set 'cfi = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // The functions: each records the type it is called as. The
  // ones a coroutine is split into share its record, and are
  // told apart by the suffix the split names them with: the
  // frame's resume slot holds ".resume", and its destroy slot
  // ".destroy", or ".cleanup" when the frame was not allocated.
  for (auto &fn : llvm_mod) {
    auto type = static_cast<llvm::ConstantInt*>(nullptr);
    if (const auto md = fn.getMetadata(kCfiMd)) { type = CfiTypeAt(*md, 0); }
    else if (const auto coro = fn.getMetadata(kCfiCoroMd)) {
      const auto name = fn.getName();
      if (name.ends_with(".resume")) { type = CfiTypeAt(*coro, 0); }
      else if (name.ends_with(".destroy") or name.ends_with(".cleanup")) { type = CfiTypeAt(*coro, 1); }
    }
    if (type != nullptr) {
      fn.setMetadata(llvm::LLVMContext::MD_kcfi_type, llvm::MDNode::get(ctx, llvm::ConstantAsMetadata::get(type)));
    }
  }

  // The calls carry their checks from where they were emitted
  // ("CfiFnCallBundles", "CfiGenCallBundles"). One that does not
  // would be the call an attacker could take anywhere, so it
  // stops the build.
  for (auto &fn : llvm_mod) {
    for (auto &inst : llvm::instructions(fn)) {
      const auto call = llvm::dyn_cast<llvm::CallBase>(&inst);
      if (call == nullptr or not call->isIndirectCall()) { continue; }
      if (not call->getOperandBundle(llvm::LLVMContext::OB_kcfi).has_value()) {
        spp::Ice("an indirect call in " + fn.getName().str() + " has no control-flow-integrity type");
      }
    }
  }

  // The backend emits each function's type, and each call's
  // check, only for a module that asks for them.
  llvm_mod.addModuleFlag(llvm::Module::Override, "kcfi", 1);
  return true;
}

auto spp::codegen::ApplyJumpTableBounds(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  const auto &triple = TargetTriple();
  if (not triple.isX86() and not triple.isAArch64()) {
    llvm::errs()
      << "This target cannot keep a jump table's index in bounds.\n"
      << "  To build without it, set 'jump_table_bounds = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // Read by the backend's switch lowering, so stamping them
  // after optimisation still takes effect.
  const auto attr = triple.isAArch64() ? kJumpTableHardeningAttr : kNoJumpTablesAttr;
  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration()) { continue; }
    if (attr == kNoJumpTablesAttr) { fn.addFnAttr(attr, "true"); }
    else { fn.addFnAttr(attr); }
  }
  return true;
}

auto spp::codegen::ApplyFfiUnwindGuard(
  void *llvm_module) -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  const auto i32_ty = llvm::Type::getInt32Ty(ctx);
  const auto i64_ty = llvm::Type::getInt64Ty(ctx);
  const auto ptr_ty = llvm::PointerType::get(ctx, 0);

  // Collected first, as guarding a call splits the block it is
  // in. A "musttail" call has nowhere to land, so it stays.
  auto calls = Vec<llvm::CallInst*>();
  for (auto &fn : llvm_mod) {
    for (auto &inst : llvm::instructions(fn)) {
      const auto call = llvm::dyn_cast<llvm::CallInst>(&inst);
      if (call == nullptr or call->isMustTailCall()) { continue; }
      const auto callee = call->getCalledFunction();
      if (callee != nullptr and callee->hasFnAttribute(kFfiFnAttr)) { calls.emplace_back(call); }
    }
  }
  if (calls.empty()) { return 0; }

  // C's personality: it runs cleanups, which is all a guard
  // is, and it is libgcc's, which every program already loads.
  // An exception nothing catches is never handed to a cleanup,
  // and ends in "std::terminate" instead - an abort either way.
  auto personality = llvm_mod.getOrInsertFunction(
    "__gcc_personality_v0", llvm::FunctionType::get(i32_ty, {}, true));
  auto check_failed = llvm_mod.getOrInsertFunction(
    "sppc_check_failed", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i32_ty, i64_ty, i64_ty}, false));
  if (const auto fn = llvm::dyn_cast<llvm::Function>(check_failed.getCallee())) {
    fn->setDoesNotReturn();
    fn->setDoesNotThrow();
    fn->addFnAttr(llvm::Attribute::Cold);
  }

  // One landing pad per call, so the report can name the
  // function the exception came out of. 7 is the runtime's
  // SPPC_CHECK_FFI_UNWIND (RuntimeCheck::FfiUnwind).
  for (const auto call : calls) {
    const auto fn = call->getFunction();
    if (not fn->hasPersonalityFn()) { fn->setPersonalityFn(llvm::cast<llvm::Constant>(personality.getCallee())); }
    const auto pad_bb = llvm::BasicBlock::Create(ctx, "ffi.unwound", fn);
    auto builder = llvm::IRBuilder(pad_bb);
    builder.CreateLandingPad(llvm::StructType::get(ptr_ty, i32_ty), 0)->setCleanup(true);
    const auto name = builder.CreateGlobalString(call->getCalledFunction()->getName(), "ffi.name");
    builder.CreateCall(
      check_failed, {
        llvm::ConstantInt::get(i32_ty, 7), builder.CreatePtrToInt(name, i64_ty),
        llvm::ConstantInt::get(i64_ty, 0)
      })->setDoesNotReturn();
    builder.CreateUnreachable();
    llvm::changeToInvokeAndSplitBasicBlock(call, pad_bb);
  }
  return calls.size();
}

auto spp::codegen::ApplyStackClashProtection(
  void *llvm_module) -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();

  // The module flag as well as the attribute, because a function
  // a later pass clones or outlines reads the flag; without it a
  // frame created after this point would inherit nothing.
  llvm_mod.addModuleFlag(
    llvm::Module::Override, kProbeStackAttr, llvm::MDString::get(ctx, kProbeStackInlineAsm));

  auto stamped = 0UL;
  for (auto &fn : llvm_mod) {
    // The same two exemptions as the canary: a declaration has no
    // prologue on this side, and a naked function's prologue is
    // whatever it says it is.
    if (fn.isDeclaration() or fn.hasFnAttribute(llvm::Attribute::Naked)) { continue; }
    if (fn.hasFnAttribute(kProbeStackAttr)) { continue; }
    fn.addFnAttr(kProbeStackAttr, kProbeStackInlineAsm);
    stamped += 1;
  }
  return stamped;
}

auto spp::codegen::ApplySafeStack(
  void *llvm_module) -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  auto stamped = 0UL;
  for (auto &fn : llvm_mod) {
    // The same two exemptions as the canary and the probe: a
    // declaration has no frame on this side to split, and a naked
    // function's prologue is whatever it says it is - and a split
    // frame is nothing but prologue.
    if (fn.isDeclaration() or fn.hasFnAttribute(llvm::Attribute::Naked)) { continue; }
    if (fn.hasFnAttribute(llvm::Attribute::SafeStack)) { continue; }
    if (fn.hasFnAttribute(kNoSplitStack)) { continue; }
    fn.addFnAttr(llvm::Attribute::SafeStack);
    stamped += 1;
  }
  return stamped;
}

auto spp::codegen::ApplyUnwindTables(
  void *llvm_module) -> unsigned long {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  auto stamped = 0UL;
  for (auto &fn : llvm_mod) {
    // Async, not sync: a trap or a fault stops the program mid-
    // function, not at a call, and the walk has to start there.
    if (fn.isDeclaration()) { continue; }
    if (fn.getUWTableKind() == llvm::UWTableKind::Async) { continue; }
    fn.setUWTableKind(llvm::UWTableKind::Async);
    stamped += 1;
  }
  return stamped;
}

auto spp::codegen::AssertIntrinsicNamingIsSound() -> void {
  // Once per process, and not a walk of anything: this asks
  // llvm to mangle one intrinsic name and checks the answer,
  // because the failure being guarded against is not in any
  // particular module - it is that "Intrinsic::getName" itself
  // comes back wrong, which makes every overloaded intrinsic
  // in every module unnameable at once.
  SPP_ATTR_MAYBE_UNUSED static const auto checked = [] {
    auto ctx = llvm::LLVMContext();
    auto scratch = llvm::Module("spp.intrinsic.naming.check", ctx);
    auto *const i32 = llvm::Type::getInt32Ty(ctx);
    auto *const i1 = llvm::Type::getInt1Ty(ctx);
    auto *const fn_ty = llvm::FunctionType::get(llvm::StructType::get(ctx, {i32, i1}), {i32, i32}, false);

    constexpr auto expected = llvm::StringRef("llvm.sadd.with.overflow.i32");
    const auto actual = llvm::Intrinsic::getName(llvm::Intrinsic::sadd_with_overflow, {i32}, &scratch, fn_ty);
    if (actual == expected) { return true; }

    llvm::errs()
      << "spp: llvm is mangling intrinsic names wrongly - got '" << actual << "', expected '" << expected << "'.\n"
      << "     Every overloaded intrinsic will now fail module verification. This is the libstdc++ '_M_create'\n"
      << "     incompatibility (GCC bug 127228), which '-fvisibility-inlines-hidden' in CMakeLists.txt keeps this\n"
      << "     compiler clear of; check that flag still reaches every translation unit.\n";
    return false;
  }();

  SPP_ASSERT(checked);
}

auto spp::codegen::EmitHardeningRecord(
  void *llvm_module,
  char const *record)
  -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  const auto &triple = TargetTriple();
  const auto text = llvm::StringRef(record);

  // On ELF, a note: its owner's name and the record each padded
  // to four bytes, behind a header of their sizes and the note's
  // type, in the target's byte order.
  auto bytes = Str();
  auto section = llvm::StringRef();
  if (triple.isOSBinFormatELF()) {
    const auto little = llvm_mod.getDataLayout().isLittleEndian();
    const auto put32 = [&](const std::uint32_t v) {
      for (auto i = 0U; i < 4U; ++i) { bytes.push_back(static_cast<char>(v >> (8U * (little ? i : 3U - i)))); }
    };
    const auto owner_size = kHardeningNoteOwner.size() + 1;
    put32(static_cast<std::uint32_t>(owner_size));
    put32(static_cast<std::uint32_t>(text.size()));
    put32(kHardeningNoteType);
    bytes += kHardeningNoteOwner;
    bytes.resize(12 + NotePadded(owner_size), '\0');
    bytes += text;
    bytes.resize(12 + NotePadded(owner_size) + NotePadded(text.size()), '\0');
    section = kHardeningNoteSection;
  }
  else {
    bytes = text.str();
    section = triple.isOSBinFormatMachO() ? llvm::StringRef("__TEXT,__spp_hardening") : llvm::StringRef(kHardeningCoffSection);
  }

  // Nothing refers to it, so it is kept by being "used".
  const auto init = llvm::ConstantDataArray::getString(ctx, bytes, /*AddNull=*/false);
  const auto global = new llvm::GlobalVariable(
    llvm_mod, init->getType(), true, llvm::GlobalValue::PrivateLinkage, init, "spp.hardening");
  global->setSection(section);
  global->setAlignment(llvm::Align(4));
  llvm::appendToUsed(llvm_mod, {global});
}

auto spp::codegen::ReadHardeningRecord(
  char const *path,
  char const **error)
  -> char const* {
  static auto record = Str();
  static auto failure = Str();
  record.clear();

  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    failure = llvm::toString(object.takeError());
    *error = failure.c_str();
    return nullptr;
  }

  auto const &file = *object->getBinary();
  for (auto const &sec : file.sections()) {
    auto name = sec.getName();
    if (not name) {
      llvm::consumeError(name.takeError());
      continue;
    }
    if (*name != kHardeningNoteSection and *name != kHardeningMachOSection and *name != kHardeningCoffSection) {
      continue;
    }
    auto contents = sec.getContents();
    if (not contents) {
      failure = llvm::toString(contents.takeError());
      *error = failure.c_str();
      return nullptr;
    }

    // An ELF note's record follows its header and owner's name.
    auto data = *contents;
    if (file.isELF()) {
      const auto get32 = [&](const std::size_t at) {
        auto v = std::uint32_t{0};
        for (auto i = 0U; i < 4U; ++i) {
          const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i]));
          v |= byte << (8U * (file.isLittleEndian() ? i : 3U - i));
        }
        return v;
      };
      if (data.size() < 12) { break; }
      const auto desc_at = 12 + NotePadded(get32(0));
      const auto desc_size = get32(4);
      if (desc_at + desc_size > data.size()) { break; }
      data = data.substr(desc_at, desc_size);
    }
    record = data.str();
    break;
  }
  return record.c_str();
}

auto spp::codegen::EmitObjectFile(
  void *llvm_module, char const *path) -> bool {
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
