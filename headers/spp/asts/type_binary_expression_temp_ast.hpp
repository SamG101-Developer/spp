#ifndef TYPE_BINARY_EXPRESSION_TEMP_AST_HPP
#define TYPE_BINARY_EXPRESSION_TEMP_AST_HPP

#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeBinaryExpressionTempAst final {
    /**
     * The operator token that represents the type binary operation. This indicates the type of operation being
     * performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the type binary expression. This is the second operand.
     */
    std::unique_ptr<TypeAst> rhs;

    /**
     * Construct the TypeBinaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the type binary operation.
     * @param[in] rhs The right-hand side expression of the type binary expression.
     */
    TypeBinaryExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
};


#endif //TYPE_BINARY_EXPRESSION_TEMP_AST_HPP
