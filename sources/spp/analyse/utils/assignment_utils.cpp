module spp.analyse.utils.assignment_utils;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import std;

auto spp::analyse::utils::assignment_utils::IsIdentifier(
  Ast const *expr) -> bool {
  // Determine if the AST node is an identifier.
  return expr->To<IdentifierAst>() != nullptr;
}

auto spp::analyse::utils::assignment_utils::IsAttr(
  Ast const *expr, ScopeManager const *sm) -> bool {
  // Determine if the AST node is an attribute (ie not
  // an identifier).
  const auto *const postfix = expr->To<PostfixExpressionAst>();
  if (postfix == nullptr) { return false; }
  if (postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }

  // Perform validation on the actual attribute too.
  auto const var_symbol_outermost = sm->CurrentScope->GetVarSymbolOutermost(*expr);
  return var_symbol_outermost.first != nullptr;
}

auto spp::analyse::utils::assignment_utils::IsDeref(
  Ast const *expr) -> bool {
  // Determine if the AST node is a deref op (ie not
  // an identifier or an attribute).
  const auto *const postfix = expr->To<PostfixExpressionAst>();
  if (postfix == nullptr) { return false; }

  // Check the operator on the postfix expression ast
  // node.
  return postfix->Op->To<PostfixExpressionOperatorDerefAst>() != nullptr;
}
