module;
#include <spp/macros.hpp>

module spp.codegen.llvm_fn;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.asts.function_prototype_ast;
import spp.asts.type_ast;
import spp.codegen.llvm_alloca;
import spp.utils.types;
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
  // Its attributes come with it: an "!ffi" function has no
  // definition anywhere, so when the modules are linked this
  // declaration may be the one kept, and the "spp-ffi" marker,
  // "nounwind" or "noreturn" must not be lost with it.
  const auto mirror = llvm::Function::Create(
    target.getFunctionType(),
    llvm::Function::ExternalLinkage,
    name,
    &current_module);
  mirror->setAttributes(target.getAttributes());
  mirror->setCallingConv(target.getCallingConv());
  return mirror;
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

auto spp::codegen::BuildFnValue(
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
    auto thunk_param_tys = spp::Vec<llvm::Type*>{ptr_ty};
    for (const auto param_ty : callee->getFunctionType()->params()) { thunk_param_tys.push_back(param_ty); }

    thunk = llvm::Function::Create(
      llvm::FunctionType::get(callee->getReturnType(), thunk_param_tys, false),
      llvm::GlobalValue::InternalLinkage, thunk_name, module);
    auto builder = llvm::IRBuilder<>(llvm::BasicBlock::Create(*ctx->Context, "entry", thunk));
    auto forwarded = spp::Vec<llvm::Value*>();
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

auto spp::codegen::CoerceToFnValue(
  llvm::Value *llvm_val,
  analyse::scopes::TypeRef const &target,
  analyse::scopes::TypeRef const &source,
  analyse::scopes::ScopeManager const &sm,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // A named function carries nothing at runtime, so whatever was
  // loaded for it is dropped and the chosen overload built.
  const auto fn = llvm_val != nullptr
    ? analyse::utils::fn_values::FindFnValue(source, target, sm)
    : nullptr;
  if (fn == nullptr or fn->GetLlvmFn() == nullptr or fn->GetLlvmFn()->Target == nullptr) { return llvm_val; }
  return BuildFnValue(*fn->GetLlvmFn()->Target, ctx);
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

auto spp::codegen::ClosureEnvType(
  Vec<llvm::Type*> const &capture_tys, const StrView name, LlvmCtx *ctx) -> llvm::StructType* {
  // The ownership header first, then the captures in order.
  auto fields = Vec<llvm::Type*>{llvm::Type::getInt8Ty(*ctx->Context)};
  fields.append_range(capture_tys);
  return llvm::StructType::create(*ctx->Context, fields, name, false);
}

auto spp::codegen::EmitNewClosureEnv(
  llvm::StructType *const env_ty, const bool on_heap, const StrView name, LlvmCtx *ctx) -> llvm::Value* {
  // The stack, unless the environment may outlive this frame.
  auto env = static_cast<llvm::Value*>(nullptr);
  if (not on_heap) {
    env = LlvmEntryAlloca(env_ty, Str(name) + ".alloca", ctx);
  }
  else {
    const auto size_ty = llvm::Type::getInt64Ty(*ctx->Context);
    const auto malloc_fn = GetEmissionModule(*ctx)->getOrInsertFunction(
      "sppc_malloc", llvm::FunctionType::get(llvm::PointerType::get(*ctx->Context, 0), {size_ty}, false));
    const auto size = GetEmissionModule(*ctx)->getDataLayout().getTypeAllocSize(env_ty).getFixedValue();
    env = ctx->Builder.CreateCall(malloc_fn, {llvm::ConstantInt::get(size_ty, size)}, Str(name) + ".heap");
  }

  // Record where it lives, once: whatever frees a closure's
  // environment reads it there ("EmitReleaseClosureEnv").
  ctx->Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), on_heap ? 1 : 0), env);
  return env;
}

auto spp::codegen::ClosureEnvCaptureIndex(
  const std::size_t i) -> unsigned {
  // Past the ownership header.
  return static_cast<unsigned>(i + 1);
}

auto spp::codegen::EmitReleaseClosureEnv(
  llvm::Value *const env, LlvmCtx *ctx) -> void {
  // A closure capturing nothing has no environment; one whose
  // environment is on the stack does not own it. Only the rest
  // are freed. The header is the first byte, so this needs no
  // knowledge of which closure the environment belongs to.
  auto &builder = ctx->Builder;
  const auto fn = builder.GetInsertBlock()->getParent();
  const auto check_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.check", fn);
  const auto free_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.free", fn);
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.done", fn);
  builder.CreateCondBr(builder.CreateIsNull(env), done_bb, check_bb);

  builder.SetInsertPoint(check_bb);
  const auto owned = builder.CreateLoad(llvm::Type::getInt8Ty(*ctx->Context), env, "closure.env.owned");
  builder.CreateCondBr(builder.CreateIsNotNull(owned), free_bb, done_bb);

  builder.SetInsertPoint(free_bb);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto llvm_free = GetEmissionModule(*ctx)->getOrInsertFunction(
    "sppc_free", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {ptr_ty}, false));
  builder.CreateCall(llvm_free, {env});
  builder.CreateBr(done_bb);

  builder.SetInsertPoint(done_bb);
}
