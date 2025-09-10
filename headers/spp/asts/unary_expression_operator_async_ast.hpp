#pragma once
#include <spp/asts/unary_expression_operator_ast.hpp>


struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c async keyword that indicates an asynchronous operation. This is used to mark the following function call
     * as called asynchronously.
     */
    std::unique_ptr<TokenAst> tok_async;

    /**
     * Construct the UnaryExpressionOperatorAsyncAst with the arguments matching the members.
     * @param tok_async The @c async keyword that indicates an asynchronous operation.
     */
    explicit UnaryExpressionOperatorAsyncAst(
        decltype(tok_async) &&tok_async);

    ~UnaryExpressionOperatorAsyncAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
