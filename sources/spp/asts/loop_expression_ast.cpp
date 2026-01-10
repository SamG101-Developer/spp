module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import llvm;


spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    PrimaryExpressionAst(),
    tok_loop(std::move(tok_loop)),
    body(std::move(body)),
    else_block(std::move(else_block)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_loop, lex::SppTokenType::KW_LOOP, "loop");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->body);
}


spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;


auto spp::asts::LoopExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
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
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                .with_args(*exit_expr, *loop_type, *final_member, *else_type)
                .raises_from(sm->current_scope);
        }
    }

    // Return the loop type.
    return loop_type;
}
