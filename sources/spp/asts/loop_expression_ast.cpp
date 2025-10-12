#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/loop_expression_ast.hpp>
#include <spp/asts/loop_condition_boolean_ast.hpp>
#include <spp/asts/loop_else_statement_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include "spp/analyse/utils/mem_utils.hpp"


spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(cond) &&cond,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    PrimaryExpressionAst(),
    tok_loop(std::move(tok_loop)),
    cond(std::move(cond)),
    body(std::move(body)),
    else_block(std::move(else_block)) {
}


spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;


auto spp::asts::LoopExpressionAst::pos_start() const -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopExpressionAst::pos_end() const -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::LoopExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LoopExpressionAst>(
        ast_clone(tok_loop),
        ast_clone(cond),
        ast_clone(body),
        ast_clone(else_block));
}


spp::asts::LoopExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop).append(" ");
    SPP_STRING_APPEND(cond).append(" ");
    SPP_STRING_APPEND(body).append("\n");
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(body).append("\n");
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
}


auto spp::asts::LoopExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create the loop scope.
    auto scope_name = analyse::scopes::ScopeBlockName("<loop-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    cond->stage_7_analyse_semantics(sm, meta);

    // Set the loop level information into the "meta" object.
    meta->save();
    meta->current_loop_depth += 1;
    meta->current_loop_ast = this;
    body->stage_7_analyse_semantics(sm, meta);
    if (meta->loop_return_types->contains(meta->current_loop_depth - 1)) {
        m_loop_exit_type_info = (*meta->loop_return_types)[meta->current_loop_depth - 1];
    }
    meta->restore();

    // Analyse the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_7_analyse_semantics(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the loop scope.
    sm->move_to_next_scope();

    // Check twice so that invalidation fails on the second loop.
    // Todo: use the "reset" on "sm" like in TypeStatementAst?
    auto tm = ScopeManager(sm->global_scope, sm->current_scope);
    tm.reset(sm->current_scope, sm->m_it);
    for (auto &m : {sm, &tm}) {
        meta->save();
        meta->loop_double_check_active = true;
        cond->stage_8_check_memory(m, meta);
        meta->restore();

        body->stage_8_check_memory(m, meta);
    }

    // Check the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_8_check_memory(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the loop's exit type (or Void if there are no exits fro inside the loop).
    auto [exit_expr, loop_type, _] = m_loop_exit_type_info.has_value()
        ? *m_loop_exit_type_info
        : std::make_tuple(nullptr, generate::common_types::void_type(pos_start()), nullptr);
    exit_expr = exit_expr ? exit_expr : this;

    // Check the else block's type is the same as the loop exit type.
    if (else_block != nullptr and not meta->ignore_missing_else_branch_for_inference) {
        const auto else_type = else_block->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*loop_type, *else_type, *sm->current_scope, *sm->current_scope)) {
            const auto final_member = else_block->body->final_member();
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *exit_expr, *loop_type, *final_member, *else_type).with_scopes({sm->current_scope}).raise();
        }
    }

    // If the return type is "Void", but the condition is comptime "true", then the loop is infinite.
    // Todo: change this to count the number of "exit" statements, as "exit" (void) is possible.
    if (analyse::utils::type_utils::symbolic_eq(*loop_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope)) {
        if (ast_cast<LoopConditionBooleanAst>(cond.get())) {
            // Non-symbolic expressions, or compile time symbolic boolean expressions, can never change (infinite loop)
            const auto is_symbolic = ast_cast<IdentifierAst>(cond.get());
            if (not is_symbolic or sm->current_scope->get_var_symbol(is_symbolic->shared_from_this())->memory_info->ast_comptime != nullptr) {
                loop_type = generate::common_types::never_type(pos_start());
            }
        }
    }

    // Return the loop type.
    return loop_type;
}
