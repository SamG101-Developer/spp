#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::TypePostfixExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypePostfixExpressionOperatorNestedTypeAst;
    friend struct TypePostfixExpressionOperatorOptionalAst;

    auto operator<=>(TypePostfixExpressionOperatorAst const &) const -> std::strong_ordering;

    auto operator==(TypePostfixExpressionOperatorAst const &) const -> bool;

protected:
    virtual auto equals(TypePostfixExpressionOperatorAst const &) const -> std::strong_ordering = 0;

    virtual auto equals_nested_type(TypePostfixExpressionOperatorNestedTypeAst const &) const -> std::strong_ordering;

    virtual auto equals_optional(TypePostfixExpressionOperatorOptionalAst const &) const -> std::strong_ordering;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
