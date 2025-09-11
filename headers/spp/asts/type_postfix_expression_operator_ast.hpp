#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::TypePostfixExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypePostfixExpressionOperatorNestedTypeAst;
    friend struct TypePostfixExpressionOperatorOptionalAst;

    friend auto operator<=>(TypePostfixExpressionOperatorAst const &lhs, TypePostfixExpressionOperatorAst const &rhs) -> std::weak_ordering {
        return lhs.equals(rhs);
    }

    friend auto operator==(TypePostfixExpressionOperatorAst const &lhs, TypePostfixExpressionOperatorAst const &rhs) -> bool {
        return (lhs.equals(rhs) == std::weak_ordering::equivalent);
    }

protected:
    virtual auto equals(TypePostfixExpressionOperatorAst const &) const -> std::weak_ordering = 0;

    virtual auto equals_nested_type(TypePostfixExpressionOperatorNestedTypeAst const &) const -> std::weak_ordering;

    virtual auto equals_optional(TypePostfixExpressionOperatorOptionalAst const &) const -> std::weak_ordering;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
