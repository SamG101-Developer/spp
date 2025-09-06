#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/loop_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LoopControlFlowStatementAst::LoopControlFlowStatementAst(
    decltype(tok_seq_exit) &&tok_seq_exit,
    decltype(tok_skip) &&tok_skip,
    decltype(expr) &&expr) :
    StatementAst(),
    tok_seq_exit(std::move(tok_seq_exit)),
    tok_skip(std::move(tok_skip)),
    expr(std::move(expr)) {
}


spp::asts::LoopControlFlowStatementAst::~LoopControlFlowStatementAst() = default;


auto spp::asts::LoopControlFlowStatementAst::pos_start() const -> std::size_t {
    return tok_seq_exit.empty() ? tok_skip->pos_start() : tok_seq_exit.front()->pos_start();
}


auto spp::asts::LoopControlFlowStatementAst::pos_end() const -> std::size_t {
    return expr ? expr->pos_end() : tok_skip ? tok_skip->pos_end() : tok_seq_exit.back()->pos_end();
}


auto spp::asts::LoopControlFlowStatementAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LoopControlFlowStatementAst>(
        ast_clone_vec(tok_seq_exit),
        ast_clone(tok_skip),
        ast_clone(expr));
}


spp::asts::LoopControlFlowStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(tok_seq_exit);
    SPP_STRING_APPEND(tok_skip);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::LoopControlFlowStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(tok_seq_exit);
    SPP_PRINT_APPEND(tok_skip);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::LoopControlFlowStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Get the number of control flow statements, and the loop's nesting level.
    const auto has_skip = tok_skip != nullptr;
    const auto num_controls = tok_seq_exit.size() + (has_skip ? 1 : 0);
    const auto nested_loop_depth = meta->current_loop_depth;

    // Analyse the expression if it is present.
    if (expr != nullptr) {
        ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    }

    // Check the depth of the loop is greater than or equal to the number of control statements.
    if (num_controls > nested_loop_depth) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppLoopTooManyControlFlowStatementsError>().with_args(
            *meta->current_loop_ast->tok_loop, *this, num_controls, nested_loop_depth).with_scopes({sm->current_scope}).raise();
    }

    // Save and compare the loop's "exiting" type against other nested loop's exit statement types.
    if (not has_skip) {
        expr->stage_7_analyse_semantics(sm, meta);
        const auto expr_type = expr != nullptr ? expr->infer_type(sm, meta) : nullptr;

        // Insert of check the depth's corresponding exit type.
        const auto depth = nested_loop_depth - num_controls;
        if (meta->loop_return_types.contains(depth)) {
            // If the type is already set, check it matches the current expression's type.
            auto [that_expr, that_expr_type, that_scope] = meta->loop_return_types.at(depth);
            if (not analyse::utils::type_utils::symbolic_eq(*expr_type, *that_expr_type, *sm->current_scope, *that_scope)) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                    *expr, *expr_type, *that_expr, *that_expr_type).with_scopes({sm->current_scope, that_scope}).raise();
            }
        }
    }
}


auto spp::asts::LoopControlFlowStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the expression if it is present.
    if (expr != nullptr) {
        expr->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *expr, *tok_seq_exit.back(), *sm, true, true, true, true, true, true, meta);
    }
}
