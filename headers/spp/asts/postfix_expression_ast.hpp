#ifndef POSTFIX_EXPRESSION_AST_HPP
#define POSTFIX_EXPRESSION_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::PostfixExpressionAst final : ExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left-hand side expression of the postfix expression. This is the base expression on which the postfix operation
     * is applied.
     */
    std::unique_ptr<ExpressionAst> lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the postfix expression.
     * @param[in] tok_op The operator token that represents the postfix operation.
     */
    PostfixExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op);
};


#endif //POSTFIX_EXPRESSION_AST_HPP
