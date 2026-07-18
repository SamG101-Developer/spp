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
    asts::Ast const *expr) -> bool {
    // Determine if the AST node is an identifier.
    return expr->To<asts::IdentifierAst>() != nullptr;
}

auto spp::analyse::utils::assignment_utils::IsAttr(
    asts::Ast const *expr,
    scopes::ScopeManager const *sm) -> bool {
    // Determine if the AST node is an attribute (ie not an identifier).
    const auto *const postfix = expr->To<asts::PostfixExpressionAst>();
    if (postfix == nullptr) { return false; }
    if (postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }

    // Perform validation on the actual attribute too.
    auto const var_symbol_outermost = sm->CurrentScope->GetVarSymbolOutermost(*expr);
    return var_symbol_outermost.First != nullptr;
}

auto spp::analyse::utils::assignment_utils::IsDeref(
    asts::Ast const *expr) -> bool {
    // Determine if the AST node is an attribute (ie not an identifier).
    const auto *const postfix = expr->To<asts::PostfixExpressionAst>();
    if (postfix == nullptr) { return false; }

    // Check the operator on the postfix expression ast node.
    return postfix->Op->To<asts::PostfixExpressionOperatorDerefAst>() != nullptr;
}
