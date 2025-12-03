module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.convention_ast;
import spp.asts.type_unary_expression_operator_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
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

    auto equals(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering override;

    auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;
};


spp::asts::TypeUnaryExpressionOperatorBorrowAst::~TypeUnaryExpressionOperatorBorrowAst() = default;
