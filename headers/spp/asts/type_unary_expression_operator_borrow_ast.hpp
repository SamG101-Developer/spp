#pragma once
#include <spp/asts/type_unary_expression_operator_ast.hpp>


struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
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
    auto equals(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering override;

    auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;
};
