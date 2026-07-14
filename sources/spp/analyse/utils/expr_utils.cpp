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
import spp.asts.utils.ast_utils;
import spp.utils.strings;
import spp.utils.types;
import std;

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
    asts::ExpressionAst const &expr,
    scopes::ScopeManager const &sm,
    PrimaryExpressionOptions &&options)
    -> bool {
    // Only allow types if types are explicitly allowed, or are zero type.
    if (not options.AllowTypeAst and expr.To<asts::TypeAst>() != nullptr) {
        const auto type_sym = sm.CurrentScope->GetTypeSymbol(expr.To<asts::TypeAst>());
        return type_sym->IsZeroType();
    }

    // Only allow tokens when they're explicit allowed, like "5 + .."
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
    for (auto const &[i, member] : members | std::views::enumerate) {
        const auto ret_stmt = member->To<asts::RetStatementAst>();
        const auto loop_flow_stmt = member->To<asts::LoopControlFlowStatementAst>();
        RaiseIf<SppUnreachableCodeError>(
            (ret_stmt or loop_flow_stmt) and (member != members.Back()),
            {sm.CurrentScope}, ERR_ARGS(*member, *members[static_cast<std::size_t>(i) + 1]));
    }
}

auto spp::analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions(
    asts::IdentifierAst const &identifier,
    Vec<scopes::VariableSymbol*> const &var_symbols,
    Vec<scopes::NamespaceSymbol*> const &ns_symbols,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using spp::utils::strings::ClosestMatch;
    using errors::SppIdentifierUnknownError;

    //
    const auto v_alternatives = var_symbols | std::views::transform([](auto const &x) { return x->Name->Val; });
    const auto ns_alternatives = ns_symbols | std::views::transform([](auto const &x) { return x->Name->Val; });
    const auto alternatives = std::views::concat(v_alternatives, ns_alternatives) | std::ranges::to<Vec>();

    //
    const auto closest_match = ClosestMatch(identifier.Val, alternatives);
    Raise<SppIdentifierUnknownError>(
        {sm.CurrentScope}, ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::expr_utils::RaiseMissingTypeIdentifierAndClosestOptions(
    asts::TypeIdentifierAst const &identifier,
    Vec<scopes::TypeSymbol*> const &symbols,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using spp::utils::strings::ClosestMatch;

    //
    const auto alternatives = symbols
        | std::views::filter([](auto const &x) { return not x->Name->IsCompilerGeneratedType(); })
        | std::views::transform([](auto const &x) { return x->Name->Name; })
        | std::ranges::to<Vec>();

    //
    const auto closest_match = ClosestMatch(identifier.Name, alternatives);
    Raise<errors::SppIdentifierUnknownError>(
        {sm.CurrentScope}, ERR_ARGS(identifier, "type identifier", closest_match));
}
