module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.asts.function_prototype_ast;
import spp.asts.type_ast;
import std;

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

auto spp::codegen::BuildFunctionValue(
  llvm::Function const &target,
  LlvmCtx *ctx)
  -> llvm::Constant* {
  // The thunk takes the environment pointer and drops it, then
  // forwards everything else to the function unchanged.
  const auto module = GetEmissionModule(*ctx);
  const auto callee = GetOrAddTargetIntoCurrentModule(target, *module);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto thunk_name = callee->getName().str() + ".as_value";

  auto thunk = module->getFunction(thunk_name);
  if (thunk == nullptr) {
    auto thunk_param_tys = std::vector<llvm::Type*>{ptr_ty};
    for (const auto param_ty : callee->getFunctionType()->params()) { thunk_param_tys.push_back(param_ty); }

    thunk = llvm::Function::Create(
      llvm::FunctionType::get(callee->getReturnType(), thunk_param_tys, false),
      llvm::GlobalValue::InternalLinkage, thunk_name, module);
    auto builder = llvm::IRBuilder<>(llvm::BasicBlock::Create(*ctx->Context, "entry", thunk));
    auto forwarded = std::vector<llvm::Value*>();
    for (auto &arg : thunk->args()) {
      if (arg.getArgNo() > 0) { forwarded.push_back(&arg); }
    }

    const auto call = builder.CreateCall(callee, forwarded);
    if (callee->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(call); }
  }

  llvm::Constant *const fields[] = {thunk, llvm::ConstantPointerNull::get(ptr_ty)};
  return llvm::ConstantStruct::get(llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty}), fields);
}

auto spp::codegen::CoerceToFunctionValue(
  llvm::Value *llvm_val,
  analyse::scopes::TypeRef const &target,
  analyse::scopes::TypeRef const &source,
  analyse::scopes::ScopeManager const &sm,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // A named function carries nothing at runtime, so whatever was
  // loaded for it is dropped and the chosen overload built.
  const auto fn = llvm_val != nullptr
    ? analyse::utils::func_utils::FindFunctionValue(source, target, sm)
    : nullptr;
  if (fn == nullptr or fn->GetLlvmFunc() == nullptr or fn->GetLlvmFunc()->Target == nullptr) { return llvm_val; }
  return BuildFunctionValue(*fn->GetLlvmFunc()->Target, ctx);
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
