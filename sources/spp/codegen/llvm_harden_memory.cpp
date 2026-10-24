module;
#include <spp/macros.hpp>

module spp.codegen.llvm_harden_memory;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// Attributes for probing stacks.
    constexpr auto kProbeStackAttr = llvm::StringLiteral("probe-stack");
    constexpr auto kProbeStackInlineAsm = llvm::StringLiteral("inline-asm");
  }
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
