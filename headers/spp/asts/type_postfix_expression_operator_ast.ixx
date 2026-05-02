module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorNestedTypeAst;
}

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorAst : Ast {
    TypePostfixExpressionOperatorAst();

    ~TypePostfixExpressionOperatorAst() override;

    auto operator<=>(TypePostfixExpressionOperatorAst const &) const -> Ordering;

    auto operator==(TypePostfixExpressionOperatorAst const &) const -> bool;

    SPP_ATTR_NODISCARD virtual auto EqualsNestedType(TypePostfixExpressionOperatorNestedTypeAst const &) const -> Ordering;

    SPP_ATTR_NODISCARD virtual auto Equals(TypePostfixExpressionOperatorAst const &) const -> Ordering = 0;

    SPP_ATTR_NODISCARD virtual auto NsParts() const -> Vec<Shared<const IdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto NsParts() -> Vec<Shared<IdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto TypeParts() const -> Vec<Shared<const TypeIdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto TypeParts() -> Vec<Shared<TypeIdentifierAst>> = 0;
};
