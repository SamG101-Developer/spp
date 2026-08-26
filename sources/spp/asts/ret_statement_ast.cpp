module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.ret_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_defer;
import spp.codegen.llvm_materialize;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::RetStatementAst::RetStatementAst(
  decltype(TokRet) &&tok_ret,
  decltype(Expr) &&val) :
  TokRet(std::move(tok_ret)),
  Expr(std::move(val)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokRet, lex::SppTokenType::KW_RET, "ret", Expr ? Expr->PosStart() : 0);
  Source._OriginalRetType = nullptr;
  _RetType = nullptr;
}

spp::asts::RetStatementAst::~RetStatementAst() = default;

auto spp::asts::RetStatementAst::PosStart() const
  -> std::size_t {
  // Use the "ret" token.
  return TokRet->PosStart();
}

auto spp::asts::RetStatementAst::PosEnd() const
  -> std::size_t {
  // Use the expression if it exists, otherwise use the "ret" token.
  return Expr ? Expr->PosEnd() : TokRet->PosEnd();
}

auto spp::asts::RetStatementAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<RetStatementAst>(
    AstClone(TokRet),
    AstClone(Expr));
}

auto spp::asts::RetStatementAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokRet).append(" ");
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto spp::asts::RetStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
  using analyse::utils::type_utils::TypeEq;
  using analyse::utils::type_utils::ResolveAndSubstituteSelfType;
  using analyse::errors::SppCoroutineContainsReturnStatementError;
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::errors::SppTypeMismatchError;
  using analyse::scopes::ScopeTypeIdentifierName;
  using generate::common_types::VoidType;

  // Analyse the expression.
  RaiseIf<SppInvalidPrimaryExpressionError>(
    Expr and not IsPrimaryExprTypeValid(*Expr, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Expr));

  // Check the enclosing function is a subroutine and not a subroutine, if a value is being returned.
  const auto function_flavour = meta->EnclosingFunctionFlavour;
  RaiseIf<SppCoroutineContainsReturnStatementError>(
    function_flavour->TokenType != lex::SppTokenType::KW_FUN and Expr != nullptr,
    {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokRet));

  // Analyse the expression if it exists, and determine the type of the expression.
  auto expr_type = VoidType(PosStart());
  _RetType = VoidType(PosStart());
  if (Expr != nullptr) {
    meta->Save();

    // For case conditions, we need an assignment target in case of variants. Closures have no declared return
    // type (it is inferred from the "ret" expression), so there may be no assignment target type available.
    meta->AssignmentTargetType = meta->EnclosingFunctionRetType.IsEmpty()
      ? nullptr
      : meta->EnclosingFunctionRetType.Back();
    if (meta->AssignmentTargetType != nullptr) {
      meta->AssignmentTargetType = ResolveAndSubstituteSelfType(
        *meta->AssignmentTargetType, *sm->CurrentScope, *sm, *meta);
    }
    meta->AssignmentTarget = meta->AssignmentTargetType
      ? IdentifierAst::FromType(*meta->AssignmentTargetType)
      : nullptr;
    SPP_RETURN_TYPE_OVERLOAD_HELPER(Expr.get()) { meta->ReturnTypeOverloadResolverType = meta->AssignmentTargetType; }

    Expr->Stage7_AnalyseSemantics(sm, meta);
    expr_type = Expr->InferType(sm, meta);

    _RetType = meta->AssignmentTargetType;
    Source._OriginalRetType = meta->EnclosingFunctionSourceRetType.IsEmpty()
      ? nullptr
      : meta->EnclosingFunctionSourceRetType[0];
    meta->Restore();
  }

  // Functions provide the return type, closures require inference; handle the inference.
  if (meta->EnclosingFunctionRetType.IsEmpty()) {
    _RetType = expr_type;
    Source._OriginalRetType = _RetType;
    meta->EnclosingFunctionRetType.EmplaceBack(_RetType);
    meta->EnclosingFunctionSourceRetType.EmplaceBack(_RetType);
  }

  // Type check the expression type against the return type of the enclosing subroutine.
  if (function_flavour->TokenType == lex::SppTokenType::KW_FUN) {
    const auto direct_match = TypeEq(*_RetType, *expr_type, *meta->EnclosingFunctionScope, *sm->CurrentScope);
    const auto expr_for_err = Expr ? Expr->To<Ast>() : TokRet->To<Ast>();
    RaiseIf<SppTypeMismatchError>(
      not direct_match, {meta->EnclosingFunctionScope, sm->CurrentScope},
      ERR_ARGS(*Source._OriginalRetType, *_RetType, *expr_for_err, *expr_type));
  }
}

auto spp::asts::RetStatementAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // Ensure the argument isn't moved or partially moved (for all conventions)
  if (Expr != nullptr) {
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *TokRet, *sm, true, true, true, true, meta);
  }

  // A "ret" leaves every scope up to the function at once, so no
  // closing brace is ever reached for them and their own scope-exit
  // checks never run against this path. Checked after the returned
  // value moves, so returning a value counts as consuming it.
  analyse::utils::linear_utils::CheckLiveUpToFunction(
    *TokRet, "Return", *sm, meta);
}

auto spp::asts::RetStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Mark the frame as returned either way, so the statements after the "case" this "ret" may sit inside are not
  // resolved on top of it.
  meta->CmpReturned = true;
  if (Expr == nullptr) { return; }

  // Resolve the expression.
  Expr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::RetStatementAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Inside a coroutine, "ret" ends the generator rather than
  // returning anything: control goes to the final suspend
  // block, which runs "llvm.coro.end" and hands the frame
  // handle back to whoever resumed it.
  if (meta->LlvmGenerator != nullptr and meta->LlvmGenerator->SuspendBlock != nullptr) {
    ctx->Builder.CreateBr(meta->LlvmGenerator->SuspendBlock);
    return nullptr;
  }

  // Use the return void instruction if there is no return value.
  if (Expr == nullptr) {
    codegen::EmitDeferredUnwind(
      *sm->CurrentScope, meta->EnclosingFunctionScope, true, sm, meta, ctx);
    ctx->Builder.CreateRetVoid();
    return nullptr;
  }

  // A function returning a variant may return any one of its members, or a narrower variant, so the value has to be
  // coerced into the return variant before it leaves the function.
  const auto uid = "." + spp::utils::Uid(this);
  const auto ret_type = _RetType != nullptr
    ? _RetType
    : meta->EnclosingFunctionRetType.IsEmpty()
    ? nullptr
    : meta->EnclosingFunctionRetType.Back();

  auto wrap_variant = [&](llvm::Value *llvm_ret_val) -> llvm::Value* {
    if (llvm_ret_val == nullptr or ret_type == nullptr) { return llvm_ret_val; }
    return codegen::CoerceToVariant(
      llvm_ret_val, *ret_type, *Expr->InferType(sm, meta), *sm->CurrentScope, "ret.variant" + uid, ctx);
  };

  meta->Save();
  meta->AssignmentTargetType = _RetType;
  if (meta->AssignmentTarget == nullptr) {
    meta->AssignmentTarget = MakeShared<IdentifierAst>(PosStart(), "$ret");
  }

  // The expression is always code-generated, even when its value
  // is discarded below, because it may have side effects that
  // have to happen before the function returns.
  const auto llvm_ret_val = Expr->Stage11_CodeGen(sm, meta, ctx);

  // The returned value is produced first, then every scope between here and the function's own runs what it deferred,
  // then control leaves.
  codegen::EmitDeferredUnwind(
    *sm->CurrentScope, meta->EnclosingFunctionScope, true, sm, meta, ctx);

  // A generic function instantiated so that its return type is
  // "Void" lowers to an LLVM function returning void, but its
  // body still reads "ret <expr>". Map to llvm's ret void.
  ctx->Builder.GetInsertBlock()->getParent()->getReturnType()->isVoidTy()
    ? ctx->Builder.CreateRetVoid()
    : ctx->Builder.CreateRet(wrap_variant(llvm_ret_val));
  meta->Restore();

  return nullptr;
}

auto spp::asts::RetStatementAst::Terminates() const
  -> bool {
  // This is the only statement that always terminates.
  return true;
}

SPP_MOD_END
