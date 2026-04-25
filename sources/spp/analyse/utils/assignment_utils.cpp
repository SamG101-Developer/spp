module spp.analyse.utils.assignment_utils;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
import std;


auto spp::analyse::utils::assignment_utils::is_identifier(
    asts::Ast const *expr) -> bool {
    // Determine if the AST node is an identifier.
    return expr->to<asts::IdentifierAst>() != nullptr;
}


auto spp::analyse::utils::assignment_utils::is_attr(
    asts::Ast const *expr,
    scopes::ScopeManager const *sm) -> bool {
    // Determine if the AST node is an attribute (ie not an identifier).
    return not expr->to<asts::IdentifierAst>() and sm->current_scope->get_var_symbol_outermost(*expr).first != nullptr;
}
