#ifndef TYPE_UNARY_EXPRESSION_AST_HPP
#define TYPE_UNARY_EXPRESSION_AST_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TypeUnaryExpressionOperatorAst> tok_op;

    /**
     * The type that is being operated on by the unary operator.
     */
    std::unique_ptr<TypeAst> expr;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the unary operation.
     * @param[in] expr The type that is being operated on by the unary operator.
     */
    TypeUnaryExpressionAst(
        decltype(tok_op) &&tok_op,
        decltype(expr) &&expr);
};


#endif //TYPE_UNARY_EXPRESSION_AST_HPP
