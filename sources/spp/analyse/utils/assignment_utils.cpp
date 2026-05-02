module spp.analyse.utils.assignment_utils;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
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
    if (expr->To<asts::IdentifierAst>()) { return false; }

    auto const var_symbol_outermost = sm->CurrentScope->GetVarSymbolOutermost(*expr);
    return var_symbol_outermost.First != nullptr;
}
