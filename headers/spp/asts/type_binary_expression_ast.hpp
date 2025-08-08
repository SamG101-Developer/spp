#ifndef TYPE_BINARY_EXPRESSION_AST_HPP
#define TYPE_BINARY_EXPRESSION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/temp_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeBinaryExpressionAst final : virtual Ast, mixins::TempTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left-hand side expression of the type binary expression. This is the first operand.
     */
    std::unique_ptr<TypeAst> lhs;

    /**
     * The operator token that represents the type binary operation. This indicates the type of operation being
     * performed. Either an "or" (union) or "and" (intersection) operation.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the type binary expression. This is the second operand.
     */
    std::unique_ptr<TypeAst> rhs;

    /**
     * Construct the TypeBinaryExpressionAst with the arguments matching the members.
     * @param lhs The left-hand side expression of the type binary expression.
     * @param tok_op The operator token that represents the type binary operation.
     * @param rhs The right-hand side expression of the type binary expression.
     */
    TypeBinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~TypeBinaryExpressionAst() override;

    auto convert() -> std::unique_ptr<TypeAst> override;
};


#endif //TYPE_BINARY_EXPRESSION_AST_HPP
