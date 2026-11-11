module;
#include <spp/macros.hpp>

module spp.codegen.llvm_harden_codegen;
import spp.codegen.llvm_support;
import spp.codegen.llvm_target;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
  }
}

auto spp::codegen::ApplySlsHardening(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  if (not DoesTargetSupportSlsHardening()) {
    llvm::errs()
      << "This target has no straight-line speculation hardening.\n"
      << "  To build without it, set 'sls = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // The features name the branches the trap follows: returns
  // and indirect jumps on x86, returns, indirect branches and
  // indirect calls on AArch64. Added to whatever features a
  // function already carries, as the backend reads them per
  // function rather than off the target machine. AArch64's
  // indirect calls go through thunks.
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
  if (not DoesTargetSupportRegisterScrubbing()) {
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

auto spp::codegen::ApplyFfiUnwindGuard(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  if (not DoesTargetSupportPcRelEhPointers()) {
    llvm::errs()
      << "This target cannot guard foreign calls against unwinding.\n"
      << "  To build without it, set 'ffi_unwind_guard = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }
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
  if (calls.empty()) { return true; }

  // C's personality: it runs cleanups, which is all a guard
  // is, and it is libgcc's, which every program already loads.
  // An exception nothing catches is never handed to a cleanup,
  // and ends in "std::terminate" instead - an abort either way.
  // An MSVC target unwinds with SEH instead: its C personality
  // is the MSVC runtime's, and a cleanup there is a funclet
  // ("cleanuppad"), not a landing pad.
  const auto &triple = TargetTriple();
  const auto seh = triple.isWindowsMSVCEnvironment();
  const auto personality_name = not seh
    ? "__gcc_personality_v0"
    : triple.getArch() == llvm::Triple::x86 ? "_except_handler3" : "__C_specific_handler";
  auto personality = llvm_mod.getOrInsertFunction(
    personality_name, llvm::FunctionType::get(i32_ty, {}, true));
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
    auto bundles = llvm::SmallVector<llvm::OperandBundleDef, 1>();
    if (seh) {
      // A call inside a funclet names it, or WinEHPrepare drops
      // the call as unreachable.
      const auto pad = builder.CreateCleanupPad(llvm::ConstantTokenNone::get(ctx), {});
      bundles.emplace_back("funclet", llvm::ArrayRef<llvm::Value*>(pad));
    }
    else {
      builder.CreateLandingPad(
        llvm::StructType::get(ptr_ty, i32_ty), 0)->setCleanup(true);
    }
    const auto name = builder.CreateGlobalString(call->getCalledFunction()->getName(), "ffi.name");
    builder.CreateCall(
      check_failed, {
        llvm::ConstantInt::get(i32_ty, 7), builder.CreatePtrToInt(name, i64_ty),
        llvm::ConstantInt::get(i64_ty, 0)
      }, bundles)->setDoesNotReturn();
    builder.CreateUnreachable();
    llvm::changeToInvokeAndSplitBasicBlock(call, pad_bb);
  }
  return true;
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
