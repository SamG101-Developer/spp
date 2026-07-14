module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;
}

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorAst : Ast {
    using Ast::Ast;

    ~TypeUnaryExpressionOperatorAst() override;

    auto operator<=>(TypeUnaryExpressionOperatorAst const &) const -> Ordering;

    auto operator==(TypeUnaryExpressionOperatorAst const &) const -> bool;

    SPP_ATTR_NODISCARD virtual auto EqualsOpBorrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> Ordering;

    SPP_ATTR_NODISCARD virtual auto EqualsOpNamespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> Ordering;

    SPP_ATTR_NODISCARD virtual auto Equals(TypeUnaryExpressionOperatorAst const &) const -> Ordering = 0;

    SPP_ATTR_NODISCARD virtual auto NsParts() const -> Vec<IdentifierAst*> = 0;

    SPP_ATTR_NODISCARD virtual auto TypeParts() const -> Vec<TypeIdentifierAst*> = 0;
};
