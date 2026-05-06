module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.expr_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.statement_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.strings;
import genex;
import std;

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
    asts::ExpressionAst const &expr,
    PrimaryExpressionOptions &&options)
    -> bool {
    // Expression cast tests.
    if (not options.AllowTypeAst and expr.To<asts::TypeAst>() != nullptr) { return false; }
    if (not options.AllowTokenAst and expr.To<asts::TokenAst>() != nullptr) { return false; }
    return true;
}

auto spp::analyse::utils::expr_utils::ValidateNoUnreachableCode(
    Vec<asts::StatementAst*> const &members,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using errors::SppUnreachableCodeError;

    // Check for statements after a terminating statement has been reached.
    for (auto const &[i, member] : members | genex::views::enumerate) {
        const auto ret_stmt = member->To<asts::RetStatementAst>();
        const auto loop_flow_stmt = member->To<asts::LoopControlFlowStatementAst>();
        RaiseIf<SppUnreachableCodeError>(
            (ret_stmt or loop_flow_stmt) and (member != members.Back()),
            {sm.CurrentScope}, ERR_ARGS(*member, *members[i + 1]));
    }
}

auto spp::analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions(
    asts::IdentifierAst const &identifier,
    Vec<Shared<scopes::VariableSymbol>> const &var_symbols,
    Vec<Shared<scopes::NamespaceSymbol>> const &ns_symbols,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using spp::utils::strings::ClosestMatch;

    //
    const auto v_alternatives = var_symbols
        | genex::views::transform([](auto const &x) { return x->Name->Val; })
        | genex::to<Vec>();
    const auto ns_alternatives = ns_symbols
        | genex::views::transform([](auto const &x) { return x->Name->Val; })
        | genex::to<Vec>();
    const auto alternatives = genex::views::concat(v_alternatives, ns_alternatives) | genex::to<Vec>();

    //
    const auto closest_match = ClosestMatch(identifier.Val, alternatives);
    Raise<errors::SppIdentifierUnknownError>(
        {sm.CurrentScope}, ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::expr_utils::RaiseMissingTypeIdentifierAndClosestOptions(
    asts::TypeIdentifierAst const &identifier,
    Vec<Shared<scopes::TypeSymbol>> const &symbols,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using spp::utils::strings::ClosestMatch;

    //
    const auto alternatives = symbols
        | genex::views::filter([](auto const &x) { return not x->Name->IsCompilerGeneratedType(); })
        | genex::views::transform([](auto const &x) { return x->Name->Name; })
        | genex::to<Vec>();

    //
    const auto closest_match = ClosestMatch(identifier.Name, alternatives);
    Raise<errors::SppIdentifierUnknownError>(
        {sm.CurrentScope}, ERR_ARGS(identifier, "type identifier", closest_match));
}
