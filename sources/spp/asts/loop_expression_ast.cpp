module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import :common_types;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.lex;
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
    // Get the loop's exit type (or Void if there are no exits from inside the loop).
    auto [exit_expr, loop_type, _] = m_loop_exit_type_info.has_value()
        ? *m_loop_exit_type_info
        : std::make_tuple(nullptr, common_types::void_type(pos_start()), nullptr);
    exit_expr = exit_expr ? exit_expr : this;

    // Check the else block's type is the same as the loop exit type.
    if (else_block != nullptr and not meta->ignore_missing_else_branch_for_inference) {
        const auto else_type = else_block->infer_type(sm, meta);
        const auto final_member = else_block->body->final_member();
        raise_if<analyse::errors::SppTypeMismatchError>(
            not analyse::utils::type_utils::symbolic_eq(*loop_type, *else_type, *sm->current_scope, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*exit_expr, *loop_type, *final_member, *else_type));
    }

    // Return the loop type.
    return loop_type;
}
