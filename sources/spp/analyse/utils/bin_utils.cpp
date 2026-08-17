module spp.analyse.utils.bin_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.convention_ref_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.is_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import genex;

auto spp::analyse::utils::bin_utils::CombineCompOps(
  asts::BinaryExpressionAst &bin_expr,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Unique<asts::BinaryExpressionAst> {
  // Check the left-hand-side is a binary expression with a
  // comparison operator. If there isn't a chaining combination
  // to do, return a new bin expression copying the old one.
  const auto bin_lhs = bin_expr.Lhs->To<asts::BinaryExpressionAst>();
  if (
    bin_lhs == nullptr or
    not genex::contains(kBinComparisonOps, bin_expr.TokOp->TokenType) or
    not genex::contains(kBinComparisonOps, bin_lhs->TokOp->TokenType)) {
    return MakeUnique<asts::BinaryExpressionAst>(
      std::move(bin_expr.Lhs),
      std::move(bin_expr.TokOp),
      std::move(bin_expr.Rhs));
  }

  // Non-symbolic value being reused -> put it into a variable
  // first. Todo: Standardize materialization?
  if (sm->CurrentScope->GetVarSymbolOutermost(*bin_lhs->Rhs).first == nullptr) {
    const auto temp_var_name = ( {
      const auto uid = spp::utils::Uid(bin_lhs->Rhs.get());
      MakeShared<asts::IdentifierAst>(
        bin_lhs->Rhs->PosStart(), uid);
    });

    const auto temp_let = ( {
      auto var = MakeUnique<asts::LocalVariableSingleIdentifierAst>(
        nullptr, temp_var_name, nullptr);
      MakeUnique<asts::LetStatementInitializedAst>(
        nullptr, std::move(var), nullptr, nullptr, std::move(bin_lhs->Rhs));
    });

    temp_let->Stage7_AnalyseSemantics(sm, meta);
    bin_lhs->Rhs = asts::AstClone(temp_var_name);
  }

  // Otherwise, re-arrange the ASTs, with an "and" combinator
  // binary expression.
  auto lhs = asts::AstClone(bin_lhs->Rhs);
  auto rhs = std::move(bin_expr.Rhs);
  auto op_pos = bin_expr.TokOp->PosStart();
  bin_expr.Rhs = MakeUnique<asts::BinaryExpressionAst>(
    std::move(lhs), std::move(bin_expr.TokOp), std::move(rhs));
  bin_expr.TokOp = MakeUnique<asts::TokenAst>(
    op_pos, lex::SppTokenType::KW_AND, "and");

  return CombineCompOps(bin_expr, sm, meta);
}

auto spp::analyse::utils::bin_utils::ConvertBinExprToFuncCall(
  asts::BinaryExpressionAst &bin_expr,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Unique<asts::PostfixExpressionAst> {
  // Before converting into a function check if we can chain
  // comparison operators.
  const auto new_bin_expr = CombineCompOps(bin_expr, sm, meta);

  // Get the method names based on the operator token. For
  // example, `1 + 2` is the same as `1.add(2)` (which after
  // further processing is `S32::add(1, 2)`).
  auto method_name = kBinMethods.at(new_bin_expr->TokOp->TokenType);
  auto method_name_wrapped = asts::IdentifierAst::MappedFromTok(
    *new_bin_expr->TokOp, std::move(method_name));

  // Construct the field access ast using the previously
  // determined name, and then wrap the function call operator,
  // ready for argument injection from the operands.
  auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, std::move(method_name_wrapped));
  auto field_access = MakeUnique<asts::PostfixExpressionAst>(
    std::move(new_bin_expr->Lhs), std::move(field));
  auto fn_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);

  // Inject the arguments for the function call, applying the
  // conventions in the standard way enforced by the operator
  // classes.
  auto conv = genex::contains(kBinComparisonOps, new_bin_expr->TokOp->TokenType)
    ? MakeUnique<asts::ConventionRefAst>(nullptr)
    : nullptr;
  auto arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(
    std::move(conv), nullptr, std::move(new_bin_expr->Rhs));
  fn_call->FnArgGroup->Args.EmplaceBack(std::move(arg));
  fn_call->Source.OriginalExpr = &bin_expr;

  // Finally combine the function call ast with the field access
  // ast into a postfix expression.
  auto new_ast = MakeUnique<asts::PostfixExpressionAst>(
    std::move(field_access), std::move(fn_call));
  return new_ast;
}

auto spp::analyse::utils::bin_utils::ConvertIsExprToFuncCall(
  asts::IsExpressionAst &is_expr,
  scopes::ScopeManager *,
  asts::meta::CompilerMetaData *)
  -> Unique<asts::CaseExpressionAst> {
  // Construct the expression-pattern based on the
  // right-hand-side of the "x is Type".
  auto pattern = std::move(is_expr.Rhs);
  auto patterns = Vec<Unique<asts::CasePatternVariantAst>>();
  patterns.EmplaceBack(std::move(pattern));

  // Construct the case expression branch that contains the
  // pattern, yielding "true", and an "else" branch yielding
  // "false".
  const auto pos = is_expr.PosStart();
  auto match_members = Vec<Unique<asts::StatementAst>>();
  match_members.EmplaceBack(asts::BooleanLiteralAst::True(pos));
  auto match_body = MakeUnique<asts::InnerScopeExpressionAst>(
    nullptr, std::move(match_members), nullptr);

  auto no_match_members = Vec<Unique<asts::StatementAst>>();
  no_match_members.EmplaceBack(asts::BooleanLiteralAst::False(pos));
  auto no_match_body = MakeUnique<asts::InnerScopeExpressionAst>(
    nullptr, std::move(no_match_members), nullptr);

  auto else_patterns = Vec<Unique<asts::CasePatternVariantAst>>();
  else_patterns.EmplaceBack(MakeUnique<asts::CasePatternVariantElseAst>(nullptr));

  auto branch = MakeUnique<asts::CaseExpressionBranchAst>(
    std::move(is_expr.TokOp), std::move(patterns), nullptr, std::move(match_body));
  auto else_branch = MakeUnique<asts::CaseExpressionBranchAst>(
    nullptr, std::move(else_patterns), nullptr, std::move(no_match_body));
  auto branches = Vec<Unique<asts::CaseExpressionBranchAst>>();
  branches.EmplaceBack(std::move(branch));
  branches.EmplaceBack(std::move(else_branch));

  // Construct and return the case expression AST.
  auto case_expr = MakeUnique<asts::CaseExpressionAst>(
    nullptr, std::move(is_expr.Lhs), nullptr, std::move(branches));
  case_expr->LoweredFromIsExpr = true;
  return case_expr;
}
