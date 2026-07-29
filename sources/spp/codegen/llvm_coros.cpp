module;
#include <spp/macros.hpp>

module spp.codegen.llvm_coros;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_implementation_lowered_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.utils.uid;
import llvm;
import genex;
import std;

auto spp::codegen::CollectCoroFrameVars(
  analyse::scopes::Scope const &scope)
  -> Vec<Shared<analyse::scopes::VariableSymbol>> {
  // Collect the coroutine scope's own variables first.
  auto out = Vec<Shared<analyse::scopes::VariableSymbol>>();
  for (auto const &sym : scope.AllVarSymbols(true)) {
    if (sym->IsGeneric or sym->Type == nullptr) { continue; }
    out.EmplaceBack(sym->SharedFromThis<analyse::scopes::VariableSymbol>());
  }

  // Get all the child scopes' variables too. Closure scopes are auto avoided because of how they are parent-linked to
  // the module scope.
  for (auto const &child : scope.Children) {
    if (child->NameAsString().starts_with("<closure-outer")) { continue; } // Todo: Likely remove
    for (auto &&sym : CollectCoroFrameVars(*child)) { out.EmplaceBack(std::move(sym)); }
  }
  return out;
}

namespace {
  // "View[T]"'s runtime-length iterators ("iter_ref"/"iter_mut"/"reverse_iter_ref"/"reverse_iter_mut") need a
  // persistent "{current element ptr, remaining count}" pair to track loop progress across resumes - unlike
  // "Arr[T, n]::iter_mov" (whose length is a compile-time constant, so it can unroll into one yield per element
  // instead), and unlike "Slot"/"StrView::slice_*" (which either borrow an address that already exists, or have a
  // spare same-size parameter pair to repurpose as scratch storage). None of "View"'s iterators have a second
  // parameter to reuse, so this reserves two extra fields - a pointer and a "USize" - right after the normal frame
  // fields, for exactly these four coroutines.
  auto NeedsIterScratchFields(spp::Str const &scope_path) -> bool {
    return scope_path == "std.view.View.iter_ref"
      or scope_path == "std.view.View.iter_mut"
      or scope_path == "std.view.View.reverse_iter_ref"
      or scope_path == "std.view.View.reverse_iter_mut";
  }
}

auto spp::codegen::CreateCoroEnvType(
  asts::CoroutinePrototypeAst *coro,
  LLvmCtx const *ctx,
  analyse::scopes::Scope const &scope)
  -> llvm::StructType* {
  //
  using analyse::utils::type_utils::IsTypeVoid;

  // Get the type being yielded and the type being sent back in.
  const auto [generator_type, yield_type, _] = analyse::utils::type_utils::GetGenAndYieldTypes(
    *coro->ReturnType, scope, *coro->ReturnType, "coroutine prototype");
  const auto send_type = generator_type->LastTypePart()->GnArgGroup->TypeAt("Send")->Val;

  // Fixed header fields: state (i8), location (i32), the send slot and the yield slot.
  const auto llvm_state_type = llvm::Type::getInt8Ty(*ctx->Context);
  const auto llvm_location_type = llvm::Type::getInt32Ty(*ctx->Context);
  const auto llvm_yield_type = GetLlvmType(*scope.GetTypeSymbol(yield_type.get()), ctx);

  // For Send != Void, the send slot carries the resumed value (dummy i8 otherwise, to keep field order stable).
  const auto llvm_send_type = not IsTypeVoid(*send_type, scope)
    ? GetLlvmType(*scope.GetTypeSymbol(send_type.get()), ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  // Non-generically substituted guard.
  // Todo: Do we need this?
  if (llvm_yield_type == nullptr or llvm_send_type == nullptr) { return nullptr; }

  // Header fields, in GenEnvField order.
  auto fields = Vec<llvm::Type*>{llvm_state_type, llvm_location_type, llvm_send_type, llvm_yield_type};

  // One field per frame variable (parameters and body locals), starting at GenEnvField::FRAME_START.
  // Todo: is this ignoring the borrow convention?
  for (auto const &var_sym : CollectCoroFrameVars(scope)) {
    fields.EmplaceBack(GetLlvmType(*scope.GetTypeSymbol(var_sym->Type.get()), ctx));
  }

  // A handful of "!intrinsic" coroutines (see "NeedsIterScratchFields") need working memory beyond their real
  // parameters/locals - reserve it as two extra fields, right after the frame fields, so
  // "GenEnvField::FRAME_START + CollectCoroFrameVars(scope).Len()" is always this scratch pointer's field index.
  const auto lowered_impl = coro->Impl->To<asts::FunctionImplementationLoweredAst>();
  if (lowered_impl != nullptr and NeedsIterScratchFields(lowered_impl->GetScopePtr())) {
    fields.EmplaceBack(llvm::PointerType::get(*ctx->Context, 0));
    fields.EmplaceBack(llvm::Type::getInt64Ty(*ctx->Context));
  }

  const auto coro_env_type = llvm::StructType::create(
    *ctx->Context, fields.ToStdVector(), "gen.env" + spp::utils::Uid(coro));
  coro->LlvmCoroGenEnvType = coro_env_type;
  return coro_env_type;
}

auto spp::codegen::CreateCoroResFunc(
  asts::CoroutinePrototypeAst *coro,
  LLvmCtx *ctx,
  analyse::scopes::Scope const &scope)
  -> llvm::Function* {
  // Get the env object from the coroutine prototype being called.
  const auto uid = "." + spp::utils::Uid(coro);
  const auto env_type = coro->LlvmCoroGenEnvType;
  SPP_ASSERT(env_type != nullptr);

  // The resume function signature is "(env*, send) -> void". The send type matches the env's send slot.
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto llvm_send_type = env_type->getElementType(std::to_underlying(GenEnvField::SEND_SLOT));
  const auto llvm_func_type = llvm::FunctionType::get(
    llvm::Type::getVoidTy(*ctx->Context), {ptr_ty, llvm_send_type}, false);
  const auto llvm_func_name = coro->GetLlvmFunc()->Target->getName().str() + ".coro.resume" + uid;
  const auto llvm_func = llvm::Function::Create(
    llvm_func_type, llvm::Function::InternalLinkage, llvm_func_name, ctx->Module.get());
  llvm_func->addFnAttr(llvm::Attribute::NoUnwind);
  llvm_func->addFnAttr(llvm::Attribute::NoInline);
  coro->LlvmCoroResumeFunc = llvm_func;

  // Entry block; bind the env pointer so "gen" expressions can reach the frame.
  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, llvm_func);
  ctx->Builder.SetInsertPoint(entry_bb);
  const auto env_ptr = llvm_func->getArg(0);
  coro->LlvmCoroGenEnv = env_ptr;

  // Store the sent-in value into the send slot, so the "gen" that yielded control can read it on resume.
  ctx->Builder.CreateStore(
    llvm_func->getArg(1),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::SEND_SLOT), "coro.send.slot" + uid));

  // Point every frame variable's storage at its env field, so reads/writes persist across resumes (the frame lives
  // on the caller's stack). LocalVariableSingleIdentifierAst skips creating a fresh alloca when one is pre-set.
  for (auto const &[i, var_sym] : CollectCoroFrameVars(scope) | genex::views::enumerate) {
    const auto field = std::to_underlying(GenEnvField::FRAME_START) + static_cast<unsigned>(i);
    var_sym->LlvmInfo->Alloca = ctx->Builder.CreateStructGEP(env_type, env_ptr, field, "coro.frame.gep" + uid);
  }

  return llvm_func;
}

auto spp::codegen::create_async_spawn_func(
  LLvmCtx *ctx,
  analyse::scopes::TypeSymbol const &fut_type_sym) -> llvm::Function* {
  // Create the async spawn function.
  const auto llvm_closure_func_ptr_type = llvm::PointerType::get(*ctx->Context, 0);
  const auto llvm_fut_ptr_type = llvm::PointerType::get(*ctx->Context, 0);

  const auto spawn_func_type = llvm::FunctionType::get(
    llvm::Type::getVoidTy(*ctx->Context),
    {llvm_closure_func_ptr_type, llvm_fut_ptr_type}, false);

  const auto internal_spawn_func_name = Str("async_internal_spawn") + mangle::mangle_type_name(fut_type_sym);
  if (const auto func = ctx->Module->getFunction(internal_spawn_func_name); func != nullptr) {
    return func;
  }

  const auto internal_spawn_func = llvm::Function::Create(
    spawn_func_type, llvm::Function::InternalLinkage,
    internal_spawn_func_name, ctx->Module.get());

  internal_spawn_func->addFnAttr(llvm::Attribute::NoUnwind);
  internal_spawn_func->addFnAttr(llvm::Attribute::NoInline);

  // Give the function a body: it launches a real OS thread that runs the closure concurrently with the caller. A
  // stackless "gen fn"-style state machine (the same lowering the generators use, in "CreateCoroResFunc" above) or
  // raw "llvm.coro.*" intrinsics only give suspend/resume on the *same* thread - they don't provide the concurrency
  // an "async" call needs while the caller carries on, so this reuses the real pthread primitive the rest of the
  // runtime is already built on (see "std::threading::thread::spawn", which is backed by the same "sppc" FFI).
  const auto save_ip = ctx->Builder.saveIP();

  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry", internal_spawn_func);
  ctx->Builder.SetInsertPoint(entry_bb);

  const auto closure_fn_arg = internal_spawn_func->getArg(0);
  const auto fut_arg = internal_spawn_func->getArg(1);

  // Build the closure value as this compiler's own { fn_ptr, env_ptr } fat-pointer convention (see the closure-call
  // codegen in "PostfixExpressionOperatorFunctionCallAst"), so it can be handed straight to "sppc_pthread_create",
  // whose "start_routine" parameter is a "FunMov[(), CVoid]" closure using that exact same layout.
  const auto closure_ty = llvm::StructType::get(*ctx->Context, {llvm_closure_func_ptr_type, llvm_fut_ptr_type});
  auto closure_val = static_cast<llvm::Value*>(llvm::UndefValue::get(closure_ty));
  closure_val = ctx->Builder.CreateInsertValue(closure_val, closure_fn_arg, {0u}, "async.spawn.closure.fn");
  closure_val = ctx->Builder.CreateInsertValue(closure_val, fut_arg, {1u}, "async.spawn.closure.env");

  // Declare the two native thread primitives directly (the raw C symbols, matching the "sppc" runtime's ABI), rather
  // than going through this compiler's own "!ffi" declaration codegen for "sppc::pthread_create"/"pthread_detach".
  const auto handle_ptr_type = llvm::PointerType::get(*ctx->Context, 0);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  const auto pthread_create_type = llvm::FunctionType::get(i32_ty, {closure_ty, handle_ptr_type}, false);
  const auto pthread_create_func = ctx->Module->getFunction("sppc_pthread_create") != nullptr
    ? ctx->Module->getFunction("sppc_pthread_create")
    : llvm::Function::Create(
      pthread_create_type, llvm::Function::ExternalLinkage, "sppc_pthread_create", ctx->Module.get());

  const auto pthread_detach_type = llvm::FunctionType::get(i32_ty, {handle_ptr_type}, false);
  const auto pthread_detach_func = ctx->Module->getFunction("sppc_pthread_detach") != nullptr
    ? ctx->Module->getFunction("sppc_pthread_detach")
    : llvm::Function::Create(
      pthread_detach_type, llvm::Function::ExternalLinkage, "sppc_pthread_detach", ctx->Module.get());

  // Spawn the thread and immediately detach it: nothing joins on it directly, "Fut::await" instead spin-waits on the
  // atomic "state" field that the spawned closure sets to COMPLETED on its way out.
  const auto handle_alloca = ctx->Builder.CreateAlloca(llvm::Type::getInt64Ty(*ctx->Context), nullptr, "async.spawn.handle");
  ctx->Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctx->Context), 0), handle_alloca);
  ctx->Builder.CreateCall(pthread_create_type, pthread_create_func, {closure_val, handle_alloca});
  ctx->Builder.CreateCall(pthread_detach_type, pthread_detach_func, {handle_alloca});
  ctx->Builder.CreateRetVoid();

  ctx->Builder.restoreIP(save_ip);
  return internal_spawn_func;
}
