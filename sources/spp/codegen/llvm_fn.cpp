module;
#include <spp/macros.hpp>

module spp.codegen.llvm_fn;
import spp.analyse.scopes.symbols;
import spp.asts.type_ast;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_cfi;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_target;
import spp.utils.types;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts, TypeAst);

namespace spp::codegen {
  /// The header every closure's environment starts with: "owns"
  /// and "drop_captures" ("ClosureEnvType").
  static auto ClosureEnvHeaderType(LlvmCtx const *ctx) -> llvm::StructType*;
}

auto spp::codegen::ClosureEnvHeaderType(LlvmCtx const *ctx) -> llvm::StructType* {
  return llvm::StructType::get(
    *ctx->Context, {llvm::Type::getInt8Ty(*ctx->Context), llvm::PointerType::get(*ctx->Context, 0)});
}

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

auto spp::codegen::LoadGlobalConstant(
  LlvmVarSymbolInfo const &info, Str const &name, LlvmCtx *ctx) -> llvm::Value* {
  // Declared here if it was defined in another module, and read
  // as its symbol's type when its initializer has another shape.
  const auto global_var = GetOrAddGlobalIntoCurrentModule(
    *llvm::cast<llvm::GlobalVariable>(info.Alloca), *ctx->Module);
  const auto value_type = info.ValueType != nullptr ? info.ValueType : global_var->getValueType();
  return ctx->Builder.CreateLoad(value_type, global_var, name);
}

auto spp::codegen::BuildFnValue(
  llvm::Function const &target, TypeRef const &fn_sup,
  LlvmCtx const *ctx) -> llvm::Constant* {
  // The thunk takes the environment pointer and drops it, then
  // forwards everything else to the function unchanged. One
  // function can be the value of two function types, which are
  // two types to call it as, so the thunk is per type.
  const auto llvm_mod = ctx->Module.get();
  const auto callee = GetOrAddTargetIntoCurrentModule(target, *llvm_mod);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  auto type_hex = Str(8, '0');
  auto t = CfiFnType(fn_sup);
  for (auto i = 8uz; i > 0; --i, t >>= 4) { type_hex[i - 1] = "0123456789abcdef"[t & 0xfu]; }
  const auto thunk_name = callee->getName().str() + ".as_value." + type_hex;

  auto thunk = llvm_mod->getFunction(thunk_name);
  if (thunk == nullptr) {
    auto thunk_param_tys = spp::Vec<llvm::Type*>{ptr_ty};
    for (const auto param_ty : callee->getFunctionType()->params()) { thunk_param_tys.push_back(param_ty); }

    thunk = llvm::Function::Create(
      llvm::FunctionType::get(callee->getReturnType(), thunk_param_tys, false),
      llvm::GlobalValue::InternalLinkage, thunk_name, llvm_mod);
    StampCfiFn(*thunk, fn_sup);
    auto builder = llvm::IRBuilder(llvm::BasicBlock::Create(*ctx->Context, "entry", thunk));
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

auto spp::codegen::ClosureEnvType(
  Vec<llvm::Type*> const &capture_tys, const StrView name,
  LlvmCtx const *ctx) -> llvm::StructType* {
  // The header first ("owns", "drop_captures"), then the captures in order.
  auto fields = Vec<llvm::Type*>{llvm::Type::getInt8Ty(*ctx->Context), llvm::PointerType::get(*ctx->Context, 0)};
  fields.append_range(capture_tys);
  return llvm::StructType::create(*ctx->Context, fields, name, false);
}

auto spp::codegen::EmitNewClosureEnv(
  llvm::StructType *const env_ty, const bool on_heap, llvm::Function *const drop_captures,
  const StrView name, LlvmCtx *ctx) -> llvm::Value* {
  // The stack, unless the environment may outlive this frame.
  auto env = static_cast<llvm::Value*>(nullptr);
  if (not on_heap) {
    env = LlvmEntryAlloca(env_ty, Str(name) + ".alloca", ctx);
  }
  else {
    const auto size_ty = TargetSizeType(*ctx->Context);
    const auto malloc_fn = ctx->Module->getOrInsertFunction(
      "sppc_malloc", llvm::FunctionType::get(llvm::PointerType::get(*ctx->Context, 0), {size_ty}, false));
    const auto size = ctx->Module->getDataLayout().getTypeAllocSize(env_ty).getFixedValue();
    env = ctx->Builder.CreateCall(malloc_fn, {llvm::ConstantInt::get(size_ty, size)}, Str(name) + ".heap");
  }

  // Record where it lives, and what drops its captures, once:
  // whatever frees or drops a closure's environment reads them
  // there ("EmitReleaseClosureEnv", "EmitDropClosureEnv").
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  ctx->Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), on_heap ? 1 : 0), env);
  ctx->Builder.CreateStore(
    drop_captures != nullptr
      ? static_cast<llvm::Value*>(drop_captures)
      : static_cast<llvm::Value*>(llvm::ConstantPointerNull::get(ptr_ty)),
    ctx->Builder.CreateStructGEP(ClosureEnvHeaderType(ctx), env, 1, Str(name) + ".drop_captures"));
  return env;
}

auto spp::codegen::ClosureEnvCaptureIndex(
  const std::size_t i) -> unsigned {
  // Past the header ("owns", "drop_captures").
  return static_cast<unsigned>(i + 2);
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
  const auto llvm_free = ctx->Module->getOrInsertFunction(
    "sppc_free", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {ptr_ty}, false));
  builder.CreateCall(llvm_free, {env});
  builder.CreateBr(done_bb);

  builder.SetInsertPoint(done_bb);
}

auto spp::codegen::EmitDropClosureEnv(
  llvm::Value *const env, LlvmCtx *ctx) -> void {
  // A closure capturing nothing has no environment, and one
  // whose captures need nothing has no destructor in its header.
  // The rest have their captures dropped before the storage holding them is freed.
  auto &builder = ctx->Builder;
  const auto fn = builder.GetInsertBlock()->getParent();
  const auto read_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.drop.read", fn);
  const auto call_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.drop.call", fn);
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "closure.env.drop.done", fn);
  builder.CreateCondBr(builder.CreateIsNull(env), done_bb, read_bb);

  builder.SetInsertPoint(read_bb);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto drop_captures = builder.CreateLoad(
    ptr_ty, builder.CreateStructGEP(ClosureEnvHeaderType(ctx), env, 1), "closure.env.drop_captures");
  builder.CreateCondBr(builder.CreateIsNotNull(drop_captures), call_bb, done_bb);

  builder.SetInsertPoint(call_bb);
  builder.CreateCall(
    llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {ptr_ty}, false), drop_captures, {env},
    CfiClosureEnvDropCallBundles(ctx));
  builder.CreateBr(done_bb);

  builder.SetInsertPoint(done_bb);
  EmitReleaseClosureEnv(env, ctx);
}
