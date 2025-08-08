#ifndef TYPE_UNARY_EXPRESSION_OPERATOR_BORROW_AST_HPP
#define TYPE_UNARY_EXPRESSION_OPERATOR_BORROW_AST_HPP

#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The convention token representing the borrowing convention. This indicates how the type is borrowed, immutably or
     * mutably.
     */
    std::unique_ptr<ConventionAst> conv;

    /**
    * Construct the TypeUnaryOperatorBorrowAst with the arguments matching the members.
    * @param conv The convention token representing the borrowing convention.
    */
    explicit TypeUnaryExpressionOperatorBorrowAst(
        decltype(conv) &&conv);

    auto ns_parts() const -> std::vector<IdentifierAst const *> override;

    auto type_parts() const -> std::vector<TypeIdentifierAst const *> override;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_BORROW_AST_HPP
