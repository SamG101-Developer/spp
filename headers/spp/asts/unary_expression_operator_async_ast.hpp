#ifndef UNARY_EXPRESSION_OPERATOR_ASYNC_AST_HPP
#define UNARY_EXPRESSION_OPERATOR_ASYNC_AST_HPP

#include <spp/asts/unary_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


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
};


#endif //UNARY_EXPRESSION_OPERATOR_ASYNC_AST_HPP
