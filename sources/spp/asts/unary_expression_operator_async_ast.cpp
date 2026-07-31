module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_operator_async_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
  decltype(TokAsync) &&tok_async) :
  TokAsync(std::move(tok_async)) {
}

spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosStart() const
  -> std::size_t {
  // Use the "async" token.
  return TokAsync->PosStart();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosEnd() const
  -> std::size_t {
  // Use the "async" token.
  return TokAsync->PosEnd();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<UnaryExpressionOperatorAsyncAst>(
    AstClone(TokAsync));
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokAsync).append(" ");
  SPP_STRING_END;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppAsyncTargetNotFunctionCallError;

  // Check the right-hand-side is a function call expression.
  const auto rhs = meta->UnaryExpressionRhs->To<PostfixExpressionAst>();
  RaiseIf<SppAsyncTargetNotFunctionCallError>(
    rhs == nullptr or rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>() == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*TokAsync, *meta->UnaryExpressionRhs));

  // Mark the function call as async.
  rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>()->MarkAsAsync(this);
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // We need a "Fut[T]" object to work with immediately.
  const auto uid = "." + spp::utils::Uid(this);
  const auto fut_type = InferType(sm, meta);
  const auto fut_type_sym = sm->CurrentScope->GetTypeSymbol(fut_type.get());
  const auto llvm_fut_type = llvm::cast<llvm::StructType>(codegen::GetLlvmType(*fut_type_sym, ctx));

  // Allocate the future on the caller's stack and set the initial (pending) state. No other thread can observe this
  // alloca yet (nothing has been spawned), so a plain store is fine here.
  const auto fut_alloca = codegen::LlvmEntryAlloca(llvm_fut_type, "async.fut.alloca" + uid, ctx);
  const auto fut_state_ptr = ctx->Builder.CreateStructGEP(llvm_fut_type, fut_alloca, 0, "async.fut.state_ptr" + uid);
  const auto fut_state_pending = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), 0);
  ctx->Builder.CreateStore(fut_state_pending, fut_state_ptr);

  // Building the closure's body moves the shared IR builder into a brand-new function; save the caller's insertion
  // point so the spawn call below lands back in the caller's flow rather than after the closure's own "ret".
  const auto caller_ip = ctx->Builder.saveIP();

  // Generate the async closure function that the spawned thread will run.
  const auto fut_closure_type = llvm::FunctionType::get(
    llvm::Type::getVoidTy(*ctx->Context),
    {llvm::PointerType::get(*ctx->Context, 0)}, false);

  const auto fut_closure = llvm::Function::Create(
    fut_closure_type, llvm::Function::InternalLinkage,
    "async.fut.closure" + uid, ctx->Module.get());
  fut_closure->addFnAttr(llvm::Attribute::NoUnwind);

  // Create the entry block for the closure.
  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "async.fut.closure.entry" + uid, fut_closure);
  ctx->Builder.SetInsertPoint(entry_bb);

  // Generate the function call expression inside the closure.
  meta->Save();
  meta->PreventAutoGeneratorResume = true;
  const auto fut_val = meta->UnaryExpressionRhs->Stage11_CodeGen(sm, meta, ctx);
  meta->Restore();

  const auto fut_param = fut_closure->getArg(0);

  // Write the call's result into the future's "val: Opt[T]" field (field 1) as a properly-tagged "Some", before the
  // completion flag is published, so that once an awaiter observes COMPLETED it is guaranteed to see the value too.
  const auto fut_value_ptr_in_closure = ctx->Builder.CreateStructGEP(
    llvm_fut_type, fut_param, 1, "async.fut.value_ptr.in_closure" + uid);
  const auto opt_ty = llvm::cast<llvm::StructType>(llvm_fut_type->getElementType(1));
  auto opt_val = static_cast<llvm::Value*>(llvm::UndefValue::get(opt_ty));
  if (opt_ty->getNumElements() > 1) {
    opt_val = ctx->Builder.CreateInsertValue(opt_val, fut_val, {0u}, "async.fut.opt.val" + uid);
    opt_val = ctx->Builder.CreateInsertValue(
      opt_val, llvm::ConstantInt::getBool(*ctx->Context, true), {1u}, "async.fut.opt.some" + uid);
  }
  else {
    // "Opt[Void]": there is no payload sub-object, only the presence tag.
    opt_val = ctx->Builder.CreateInsertValue(
      opt_val, llvm::ConstantInt::getBool(*ctx->Context, true), {0u}, "async.fut.opt.some" + uid);
  }
  ctx->Builder.CreateStore(opt_val, fut_value_ptr_in_closure);

  // Publish completion last, with release ordering, so the value write above happens-before any awaiting thread's
  // acquire-load of this flag observes it.
  const auto fut_state_ptr_in_closure = ctx->Builder.CreateStructGEP(
    llvm_fut_type, fut_param, 0, "async.fut.state_ptr.in_closure" + uid);
  const auto fut_state_completed = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), 1);
  const auto state_store = ctx->Builder.CreateStore(fut_state_completed, fut_state_ptr_in_closure);
  state_store->setAtomic(llvm::AtomicOrdering::Release);

  // Return from the closure.
  ctx->Builder.CreateRetVoid();

  // Restore the caller's insertion point and spawn a real OS thread to run the closure concurrently with the caller
  // (see "create_async_spawn_func" for why this uses a real thread rather than a stackless coroutine transform).
  ctx->Builder.restoreIP(caller_ip);
  const auto spawn_func = codegen::create_async_spawn_func(ctx, *fut_type_sym);
  ctx->Builder.CreateCall(spawn_func, {fut_closure, fut_alloca});

  // Return the future value immediately; the caller does not block on the async call.
  const auto fut_loaded = ctx->Builder.CreateLoad(llvm_fut_type, fut_alloca, "async.fut.loaded" + uid);
  return fut_loaded;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using generate::common_types::FutureType;

  // Wrap the function call inside a "Future" type.
  auto inner_type = meta->UnaryExpressionRhs->InferType(sm, meta);
  auto future_type = FutureType(TokAsync->PosStart(), std::move(inner_type));
  future_type->Stage7_AnalyseSemantics(sm, meta);
  return future_type;
}

SPP_MOD_END
