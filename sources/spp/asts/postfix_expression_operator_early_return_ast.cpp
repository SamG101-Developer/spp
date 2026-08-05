module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_early_return_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.gen_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
  decltype(TokQst) &&tok_qst) :
  TokQst(std::move(tok_qst)) {
}

spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosStart() const
  -> std::size_t {
  // Use the "?" token.
  return TokQst->PosStart();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosEnd() const
  -> std::size_t {
  // Use the "?" token.
  return TokQst->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Clone() const
  -> Unique<Ast> {
  auto ast = MakeUnique<PostfixExpressionOperatorEarlyReturnAst>(
    AstClone(TokQst));
  ast->_TransformedExpr = _TransformedExpr;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokQst);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppTypeMismatchError;
  using analyse::utils::type_utils::GetTryType;
  using analyse::utils::type_utils::TypeEq;
  using analyse::utils::type_utils::GetGenAndYieldTypes;

  const auto uid = "." + spp::utils::Uid(this);
  auto temp_name = MakeShared<IdentifierAst>(PosStart(), "$temp" + uid);

  // Build the materializing left-hand-side to contain the lhs of
  // the postfix expression.
  // The copy bound here is the only one that ever gets analysed - the original is left alone by
  // "PostfixExpressionAst::Stage7_AnalyseSemantics", so that the scopes the left-hand-side needs are created once,
  // inside this lowering, where stage 8 will walk them. Hold on to it: it is now the only copy that can be asked for
  // a type.
  auto lhs_copy = AstClone(meta->PostfixExpressionLhs);
  const auto analysed_lhs = lhs_copy.get();

  auto temp_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, temp_name, nullptr);
  auto let_stmt = MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(temp_var), nullptr, nullptr, std::move(lhs_copy));

  // Test for if the `Try`-superimposed type is in the value state
  // or not.
  auto is_value_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_is_value"));
  auto is_value_target = MakeUnique<PostfixExpressionAst>(
    MakeUnique<IdentifierAst>(PosStart(), temp_name->Val), std::move(is_value_field));
  auto is_value_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  is_value_call->Source.OriginalExpr = this;
  auto is_value_cond = MakeUnique<PostfixExpressionAst>(
    std::move(is_value_target), std::move(is_value_call));

  // The function call for extracting the value state out of the
  // `Try`-superimposed object, modelled as "{ $temp.op_as_value() }".
  // Move the func call into an inner scope expression { } section.
  auto output_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_as_value"));
  auto output_target = MakeUnique<PostfixExpressionAst>(
    MakeUnique<IdentifierAst>(PosStart(), temp_name->Val), std::move(output_field));
  auto output_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  output_call->Source.OriginalExpr = this;

  auto output_members = Vec<Unique<StatementAst>>();
  output_members.EmplaceBack(MakeUnique<PostfixExpressionAst>(std::move(output_target), std::move(output_call)));
  auto output_body = MakeUnique<InnerScopeExpressionAst>(nullptr, std::move(output_members), nullptr);

  // The function call for extracting the error state out of the
  // `Try`-superimposed object, modelled as "{ $temp.op_as_residual() }".
  // Move the func call into an inner scope expression { } section.
  auto residual_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_as_residual"));
  auto residual_target = MakeUnique<PostfixExpressionAst>(
    MakeUnique<IdentifierAst>(PosStart(), temp_name->Val), std::move(residual_field));
  auto residual_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  residual_call->Source.OriginalExpr = this;
  auto residual_extract = MakeUnique<PostfixExpressionAst>(
    std::move(residual_target), std::move(residual_call));

  auto residual_statements = Vec<Unique<StatementAst>>();
  if (meta->EnclosingFunctionFlavour->TokenType == lex::SppTokenType::KW_FUN) {
    residual_statements.EmplaceBack(MakeUnique<RetStatementAst>(nullptr, std::move(residual_extract)));
  }
  else {
    residual_statements.EmplaceBack(MakeUnique<GenExpressionAst>(nullptr, nullptr, std::move(residual_extract)));
    residual_statements.EmplaceBack(MakeUnique<RetStatementAst>(nullptr, nullptr));
  }
  auto residual_body = MakeUnique<InnerScopeExpressionAst>(nullptr, std::move(residual_statements), nullptr);

  // Build the "else" branch with the residual unwrap, which will get
  // returned/yielded (depending on the function flavour).
  auto else_patterns = Vec<Unique<CasePatternVariantAst>>();
  else_patterns.EmplaceBack(MakeUnique<CasePatternVariantElseAst>(nullptr));
  auto branches = Vec<Unique<CaseExpressionBranchAst>>();
  branches.EmplaceBack(
    MakeUnique<CaseExpressionBranchAst>(nullptr, std::move(else_patterns), nullptr, std::move(residual_body)));

  // Build the `case` expression with the value "branch" as the main
  // case body, and attach the else branch.
  auto case_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_CASE, "case");
  auto case_expr = CaseExpressionAst::NewNonPatternMatch(
    std::move(case_tok), std::move(is_value_cond), std::move(output_body), std::move(branches));

  // Wrap the two into one scope, so the temporary does not leak into
  // the surrounding one and the whole lowering has a single AST for
  // the later stages to forward to.
  auto members = Vec<Unique<StatementAst>>();
  members.EmplaceBack(std::move(let_stmt));
  members.EmplaceBack(std::move(case_expr));

  _TransformedExpr = MakeUnique<InnerScopeExpressionAst>(
    nullptr, std::move(members), nullptr);
  meta->Save();
  meta->AssignmentTarget = nullptr;
  meta->AssignmentTargetType = nullptr;
  meta->ReturnTypeOverloadResolverType = nullptr;
  _TransformedExpr->Stage7_AnalyseSemantics(sm, meta);
  meta->Restore();

  // The "Try" checks run against the analysed copy, and so after the lowering rather than before it. Only that copy
  // has been through stage 7, and an expression that has not cannot be asked for its type - a function call has no
  // overload picked yet, so "_OverloadInfo" is still empty.
  const auto lhs_type = analysed_lhs->InferType(sm, meta);
  const auto try_type = GetTryType(*lhs_type, *analysed_lhs, *sm);
  const auto residual_type = try_type->LastTypePart()->GnArgGroup->TypeAt("Residual")->Val;

  // Subroutine return type check.
  if (meta->EnclosingFunctionFlavour->TokenType == lex::SppTokenType::KW_FUN) {
    RaiseIf<SppTypeMismatchError>(
      not TypeEq(*meta->EnclosingFunctionRetType[0], *residual_type, *meta->EnclosingFunctionScope, *sm->CurrentScope),
      {meta->EnclosingFunctionScope, sm->CurrentScope},
      ERR_ARGS(
        *meta->EnclosingFunctionSourceRetType[0], *meta->EnclosingFunctionRetType[0], *analysed_lhs, *residual_type));
  }

  // Coroutine return type check.
  else {
    auto [_, yield_type, _] = GetGenAndYieldTypes(
      *meta->EnclosingFunctionRetType[0], *sm->CurrentScope, *analysed_lhs, "early return");
    RaiseIf<SppTypeMismatchError>(
      not TypeEq(*yield_type, *residual_type, *meta->EnclosingFunctionScope, *sm->CurrentScope),
      {meta->EnclosingFunctionScope, sm->CurrentScope},
      ERR_ARGS(*meta->EnclosingFunctionSourceRetType[0], *yield_type, *analysed_lhs, *residual_type));
  }
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Forward to the lowered form.
  _TransformedExpr->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Forward to the lowered form.
  _TransformedExpr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // Forward to the lowered form.
  return _TransformedExpr->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using analyse::utils::type_utils::GetTryType;
  if (_TransformedExpr != nullptr) {
    // Infer from the transformed ast.
    meta->Save();
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    auto transformed_type = _TransformedExpr->InferType(sm, meta);
    meta->Restore();
    return transformed_type;
  }

  // Before stage 7 there is no lowering yet, so fall back to reading it off the left-hand-side. This only works for
  // an operand that some other path has already analysed.
  const auto lhs = meta->PostfixExpressionLhs;
  const auto lhs_type = lhs->InferType(sm, meta);
  const auto try_type = GetTryType(*lhs_type, *lhs, *sm);
  return try_type->LastTypePart()->GnArgGroup->TypeAt("Value")->Val;
}

SPP_MOD_END
