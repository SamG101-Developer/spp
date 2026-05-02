module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.expr_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.expression_ast;
import spp.asts.statement_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
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
