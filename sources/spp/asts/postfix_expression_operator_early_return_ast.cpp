module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_early_return_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.type_resolution;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
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
import spp.utils.ice;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
  decltype(TokQst) &&tok_qst) :
  TokQst(std::move(tok_qst)),
  _TransformedExpr(nullptr) {
}

spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosStart() const -> std::size_t {
  // Use the "?" token.
  return TokQst->PosStart();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosEnd() const -> std::size_t {
  // Use the "?" token.
  return TokQst->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Clone() const -> Unique<Ast> {
  auto ast = MakeUnique<PostfixExpressionOperatorEarlyReturnAst>(
    AstClone(TokQst));
  ast->_TransformedExpr = _TransformedExpr;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokQst);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // A deferred expression runs at the exits of its scope,
  // so the "ret" this lowers to would have to be emitted at
  // each of them - returning out of the return that is
  // already in progress.
  RaiseIf<SppDeferTerminatesError>(
    meta->WithinDeferTok != nullptr,
    ERR_ARGS(*meta->WithinDeferTok, *this));

  // The operand is read three times (whether it holds a value, then the value
  // or the residual), so is evaluated once with hoisting: a place where it is,
  // anything else into a temporary, placed in the lowering's own scope (below).
  SPP_ASSERT(WrittenOperand != nullptr);
  auto hoisted = expr_utils::HoistOnce(*WrittenOperand, "$try", sm, meta, false);
  const auto read = [&]() -> Unique<ExpressionAst> {
    return hoisted.Name != nullptr ? AstClone(hoisted.Name) : AstClone(WrittenOperand);
  };

  // Test for if the `Try`-superimposed type is in the value
  // state or not.
  auto is_value_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_is_value"));
  is_value_field->Source.OriginalExpr = this;
  auto is_value_target = MakeUnique<PostfixExpressionAst>(
    read(), std::move(is_value_field));
  auto is_value_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  is_value_call->Source.OriginalExpr = this;
  auto is_value_cond = MakeUnique<PostfixExpressionAst>(
    std::move(is_value_target), std::move(is_value_call));

  // The function call for extracting the value state out of
  // the `Try`-superimposed object, modelled as the expression
  // "{ $temp.op_as_value() }". Move the func call into an
  // inner scope expression { } section.
  auto output_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_as_value"));
  output_field->Source.OriginalExpr = this;
  auto output_target = MakeUnique<PostfixExpressionAst>(
    read(), std::move(output_field));
  auto output_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  output_call->Source.OriginalExpr = this;

  auto output_members = Vec<Unique<StatementAst>>();
  output_members.emplace_back(MakeUnique<PostfixExpressionAst>(std::move(output_target), std::move(output_call)));
  auto output_body = MakeUnique<InnerScopeExpressionAst>(nullptr, std::move(output_members), nullptr);

  // The function call for extracting the error state out of
  // the `Try`-superimposed object, modelled as the expression
  // "{ $temp.op_as_residual() }". Move the func call into an
  // inner scope expression { } section.
  auto residual_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, MakeUnique<IdentifierAst>(PosStart(), "op_as_residual"));
  residual_field->Source.OriginalExpr = this;
  auto residual_target = MakeUnique<PostfixExpressionAst>(
    read(), std::move(residual_field));
  auto residual_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);
  residual_call->Source.OriginalExpr = this;
  auto residual_extract = MakeUnique<PostfixExpressionAst>(
    std::move(residual_target), std::move(residual_call));

  auto residual_statements = Vec<Unique<StatementAst>>();
  if (meta->EnclosingFnFlavour->TokenType == lex::SppTokenType::KW_FUN) {
    residual_statements.emplace_back(MakeUnique<RetStatementAst>(nullptr, std::move(residual_extract)));
  }
  else {
    residual_statements.emplace_back(MakeUnique<GenExpressionAst>(nullptr, nullptr, std::move(residual_extract)));
    residual_statements.emplace_back(MakeUnique<RetStatementAst>(nullptr, nullptr));
  }
  auto residual_body = MakeUnique<InnerScopeExpressionAst>(nullptr, std::move(residual_statements), nullptr);

  // Build the "else" branch with the residual unwrap, which will
  // get returned/yielded (depending on the function flavour).
  auto else_patterns = Vec<Unique<CasePatternVariantAst>>();
  else_patterns.emplace_back(MakeUnique<CasePatternVariantElseAst>(nullptr));
  auto branches = Vec<Unique<CaseExpressionBranchAst>>();
  branches.emplace_back(
    MakeUnique<CaseExpressionBranchAst>(nullptr, std::move(else_patterns), nullptr, std::move(residual_body)));

  // Build the `case` expression with the value "branch" as the
  // main case body, and attach the else branch.
  auto case_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_CASE, "case");
  auto case_expr = CaseExpressionAst::NewNonPatternMatch(
    std::move(case_tok), std::move(is_value_cond), std::move(output_body), std::move(branches));
  case_expr->LoweredFromTryOperator = true;

  // Wrap the two into one scope, so the temporary does not leak
  // into the surrounding one and the whole lowering has a single
  // AST for the later stages to forward to.
  auto members = Vec<Unique<StatementAst>>();
  if (hoisted.Let != nullptr) { members.emplace_back(std::move(hoisted.Let)); }
  members.emplace_back(std::move(case_expr));

  _TransformedExpr = MakeUnique<InnerScopeExpressionAst>(
    nullptr, std::move(members), nullptr);
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ClearAssignmentTarget();
    meta->ReturnTypeOverloadResolverType = nullptr;
    _TransformedExpr->Stage7_AnalyseSemantics(sm, meta);
  }
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the lowered form.
  _TransformedExpr->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTargetType = nullptr;
  const auto llvm_val = _TransformedExpr->Stage11_CodeGen(sm, meta, ctx);
  return llvm_val;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the lowered form.
  _TransformedExpr->EvaluateCompTime(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  //
  IMPORT_UTILS;
  if (_TransformedExpr != nullptr) {
    // Infer from the transformed ast.
    const auto _meta_guard = MetaGuard(meta);
    meta->ClearAssignmentTarget();
    auto transformed_type = _TransformedExpr->InferType(sm, meta);
    return transformed_type;
  }

  // Before stage 7 there is no lowering yet, so it is read
  // off the left-hand-side ("InferTypeRef"). This only works
  // for an operand that some other path has already analysed.
  return TypeInferrableAst::InferType(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The try type's "Value" is read off the type's arguments,
  // so only the transformed expression is forwarded.
  IMPORT_UTILS;
  if (_TransformedExpr != nullptr) {
    const auto _meta_guard = MetaGuard(meta);
    meta->ClearAssignmentTarget();
    return _TransformedExpr->InferTypeRef(sm, meta);
  }

  // Before stage 7 there is no lowering yet: the "Value" the
  // left-hand side's "Try" sup gives, as "InferType".
  const auto lhs = Lhs;
  return marker_sups::FindTrySup(
    lhs->InferTypeRef(sm, meta), *lhs, [&] { return lhs->InferType(sm, meta); }, *sm,
    "early return").Symbol->TypeArgRef("Value");
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::RebuildsOperand() const -> bool {
  // The "case" is built from the "?" operand.
  return true;
}

SPP_MOD_END
