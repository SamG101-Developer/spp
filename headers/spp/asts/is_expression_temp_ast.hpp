#ifndef IS_EXPRESSION_TEMP_AST_HPP
#define IS_EXPRESSION_TEMP_AST_HPP

#include <spp/asts/_fwd.hpp>


struct spp::asts::IsExpressionTempAst {
    /**
     * The operator token that represents the is operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the is expression. This is the second operand.
     */
    std::unique_ptr<CasePatternVariantAst> rhs;

    /**
     * Construct the IsExpressionTempAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the is operation.
     * @param[in] rhs The right-hand side expression of the is expression.
     */
    IsExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
};


#endif //IS_EXPRESSION_TEMP_AST_HPP
