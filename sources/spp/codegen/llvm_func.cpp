module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func;

auto spp::codegen::GetOrAddTargetIntoCurrentModule(
  llvm::Function const &target,
  llvm::Module &current_module)
  -> llvm::Function* {
  // Get the mangled name and check if it already declared
  // in the current module or not. Mangling uses namespaces
  // so uniqueness is guaranteed.
  const auto name = target.getName();
  if (const auto existing = current_module.getFunction(name); existing != nullptr) {
    return existing;
  }

  // Otherwise, create a matching declaration of the target
  // function in the current module for continuous reusing.
  return llvm::Function::Create(
    target.getFunctionType(),
    llvm::Function::ExternalLinkage,
    name,
    &current_module);
}

auto spp::codegen::GetOrAddGlobalIntoCurrentModule(
  llvm::GlobalVariable const &target,
  llvm::Module &current_module)
  -> llvm::GlobalVariable* {
  // Mangling is namespace qualified, so the name identifies
  // the constant across every module.
  const auto name = target.getName();
  if (const auto existing = current_module.getGlobalVariable(name, true); existing != nullptr) {
    return existing;
  }

  // Otherwise declare it here. The initializer stays with the
  // module that defines it, as repeating it would give the program
  // two definitions of one symbol. This is a bare external
  // declaration for the linker to resolve.
  const auto declaration = new llvm::GlobalVariable(
    current_module, target.getValueType(), target.isConstant(), llvm::GlobalValue::ExternalLinkage,
    nullptr, name);
  declaration->setAlignment(target.getAlign());
  return declaration;
}

auto spp::codegen::GetEmissionModule(
  LlvmCtx const &ctx)
  -> llvm::Module* {
  // Outside a function body (a "cmp" initializer, say) there is
  // nothing being emitted into, so the context's own module is
  // the only answer.
  const auto insert_bb = ctx.Builder.GetInsertBlock();
  if (insert_bb == nullptr or insert_bb->getParent() == nullptr) { return ctx.Module.get(); }
  return insert_bb->getParent()->getParent();
}
