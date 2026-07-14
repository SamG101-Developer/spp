module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
}

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The convention token representing the borrowing convention. This indicates how the type is borrowed, immutably or
     * mutably.
     */
    Unique<ConventionAst> Conv;

    /**
    * Construct the TypeUnaryOperatorBorrowAst with the arguments matching the members.
    * @param conv The convention token representing the borrowing convention.
    */
    explicit TypeUnaryExpressionOperatorBorrowAst(
        decltype(Conv) &&conv);

    ~TypeUnaryExpressionOperatorBorrowAst() override;

    SPP_ATTR_NODISCARD auto EqualsOpBorrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(TypeUnaryExpressionOperatorAst const &) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst*> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionOperatorBorrowAst)
