module;
#include <spp/macros.hpp>

module spp.codegen.llvm_entry;
import spp.codegen.llvm_harden_memory;
import spp.utils.ice;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// The name the runtime start-up shim is emitted under;
    /// dotted, so it cannot collide with a mangled s++ name.
    constexpr auto kRuntimeInitShim = llvm::StringLiteral("spp.rt.init");

    /// The name the runtime tear-down shim is emitted under;
    /// dotted, so it cannot collide with a mangled s++ name.
    constexpr auto kRuntimeCleanupShim = llvm::StringLiteral("spp.rt.cleanup");

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
  }
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
