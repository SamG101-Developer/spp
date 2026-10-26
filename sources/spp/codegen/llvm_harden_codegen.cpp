module;
#include <spp/macros.hpp>

module spp.codegen.llvm_harden_codegen;
import spp.codegen.llvm_target;
import spp.utils.ice;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// The function attribute that builds a function's
    /// switches without jump tables.
    constexpr auto kNoJumpTablesAttr = llvm::StringLiteral("no-jump-tables");

    /// The function attribute that clamps a jump table's
    /// index on AArch64 before the table is read.
    constexpr auto kJumpTableHardeningAttr = llvm::StringLiteral("aarch64-jump-table-hardening");

    /// Whether the selected target has straight-line speculation
    /// hardening: x86 and AArch64 do, nothing else does. Not
    /// AArch64 Mach-O: LLVM calls its indirect-call thunks by the
    /// unprefixed name, so every call is an undefined symbol.
    /// Todo: drop the Mach-O case once fixed; .triages/upstream-watch.md U16.
    auto TargetHasSlsHardening() -> bool {
      return TargetTriple().isX86() or (TargetTriple().isAArch64() and not TargetTriple().isOSBinFormatMachO());
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

    /// Whether LLVM writes the selected target's personality and
    /// LSDA pointers PC-relative, as a position-independent
    /// executable needs: every target but m68k, which it leaves
    /// absolute (TargetLoweringObjectFileELF has no case for it).
    auto TargetHasPcRelEhPointers() -> bool {
      return TargetTriple().getArch() != llvm::Triple::m68k;
    }

    /// The type operand "i" of a control-flow-integrity record
    /// holds.
    auto CfiTypeAt(llvm::MDNode const &md, const unsigned i) -> llvm::ConstantInt* {
      return llvm::mdconst::extract<llvm::ConstantInt>(md.getOperand(i));
    }
  }
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
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  if (not TargetHasPcRelEhPointers()) {
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
