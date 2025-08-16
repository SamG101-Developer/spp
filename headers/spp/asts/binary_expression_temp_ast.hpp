#ifndef BINARY_EXPRESSION_TEMP_AST_HPP
#define BINARY_EXPRESSION_TEMP_AST_HPP

#include <spp/asts/_fwd.hpp>


/**
 * Temporary aST node that hold the right-hand-side of a binary expression. This is because it is parsed together, aside
 * from the left-hand-side, and as an entirety may be nullptr, and discarded.
 */
struct spp::asts::BinaryExpressionTempAst {
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
     * @param[in] tok_op The operator token that represents the binary operation.
     * @param[in] rhs The right-hand side expression of the binary expression.
     */
    BinaryExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~BinaryExpressionTempAst();
};


#endif //BINARY_EXPRESSION_TEMP_AST_HPP
