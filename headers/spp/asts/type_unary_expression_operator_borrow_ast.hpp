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

    ~TypeUnaryExpressionOperatorBorrowAst() override;

protected:
    auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> bool override;

public:
    auto operator==(TypeUnaryExpressionOperatorAst const &other) const -> bool override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_BORROW_AST_HPP
