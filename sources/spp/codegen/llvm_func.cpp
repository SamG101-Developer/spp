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
