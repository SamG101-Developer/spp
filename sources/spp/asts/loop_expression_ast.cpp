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

SPP_MOD_BEGIN
spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block) :
    TokLoop(std::move(tok_loop)),
    Body(std::move(body)),
    ElseBlock(std::move(else_block)),
    _LoopExitTypeInfo(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokLoop, lex::SppTokenType::KW_LOOP, "loop");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Body);
}

spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;

auto spp::asts::LoopExpressionAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types::VoidType;
    USE_CACHED_TYPE_INFERENCE;

    // Get the loop's exit type (or Void if there are no exits from inside the loop).
    auto loop_type = _LoopExitTypeInfo ? AstClone(std::get<1>(*_LoopExitTypeInfo)) : VoidType(PosStart());
    const auto exit_expr = _LoopExitTypeInfo ? std::get<0>(*_LoopExitTypeInfo) : this;

    // Check the else block's type is the same as the loop exit type.
    if (ElseBlock != nullptr and not meta->IgnoreMissingElseBranchForInference) {
        const auto else_type = ElseBlock->InferType(sm, meta);
        const auto final_member = ElseBlock->Body->FinalMember();
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*loop_type, *else_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*exit_expr, *loop_type, *final_member, *else_type));
    }

    CACHE_TYPE_INFERENCE_AND_RETURN(loop_type);
}

SPP_MOD_END
