#pragma once
#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
    /**
     * The @c ? token that indicates an early return in a postfix expression. This token is used to signify that the
     * expression should be checked for its result-type failure type, and if it matches, the expression will lift the
     * error to the caller.
     */
    std::unique_ptr<TokenAst> tok_qst;

    /**
     * Construct the PostfixExpressionOperatorEarlyReturnAst with the arguments matching the members.
     * @param[in] tok_qst The @c ? token that indicates an early return in a postfix expression.
     */
    explicit PostfixExpressionOperatorEarlyReturnAst(
        decltype(tok_qst) &&tok_qst);

    ~PostfixExpressionOperatorEarlyReturnAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
