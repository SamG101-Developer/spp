#ifndef BINARY_EXPRESSION_AST_HPP
#define BINARY_EXPRESSION_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The BinaryExpressionAst represents a binary expression in the source code, which consists of two operands (left-hand
 * side and right-hand side) and an operator that defines the operation to be performed on those operands. Each operator
 * maps the expressions into a method on the left-hand-side type, so @code 1 + 2@endcode will map to the method
 * @code 1.add(2)@endcode.
 */
struct spp::asts::BinaryExpressionAst final : ExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left-hand side expression of the binary expression. This is the first operand.
     */
    std::unique_ptr<ExpressionAst> lhs;

    /**
     * The operator token that represents the binary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the binary expression. This is the second operand.
     */
    std::unique_ptr<ExpressionAst> rhs;

    /**
     * Construct the BinaryExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the binary expression.
     * @param[in] tok_op The operator token that represents the binary operation.
     * @param[in] rhs The right-hand side expression of the binary expression.
     */
    BinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

private:
    std::unique_ptr<PostfixExpressionAst> m_mapped_func;
};


#endif //BINARY_EXPRESSION_AST_HPP
