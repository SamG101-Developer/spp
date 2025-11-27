module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;

import std;


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypePostfixExpressionOperatorNestedTypeAst;
    friend struct TypePostfixExpressionOperatorOptionalAst;

    auto operator<=>(TypePostfixExpressionOperatorAst const &) const -> std::strong_ordering;

    auto operator==(TypePostfixExpressionOperatorAst const &) const -> bool;

    ~TypePostfixExpressionOperatorAst() override;

protected:
    virtual auto equals(TypePostfixExpressionOperatorAst const &) const -> std::strong_ordering = 0;

    virtual auto equals_nested_type(TypePostfixExpressionOperatorNestedTypeAst const &) const -> std::strong_ordering;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
