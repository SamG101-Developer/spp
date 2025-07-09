#ifndef UNARY_EXPRESSION_AST_HPP
#define UNARY_EXPRESSION_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::UnaryExpressionAst final : ExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<UnaryExpressionOperatorAst> tok_op;

    /**
     * The expression that is being operated on by the unary operator.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the unary operation.
     * @param[in] expr The expression that is being operated on by the unary operator.
     */
    UnaryExpressionAst(
        decltype(tok_op) &&tok_op,
        decltype(expr) &&expr);
};


#endif //UNARY_EXPRESSION_AST_HPP
