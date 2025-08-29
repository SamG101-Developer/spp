#ifndef TYPE_POSTFIX_EXPRESSION_OPERATOR_AST_HPP
#define TYPE_POSTFIX_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypePostfixExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypePostfixExpressionOperatorNestedTypeAst;
    friend struct TypePostfixExpressionOperatorOptionalAst;

    friend auto operator==(TypePostfixExpressionOperatorAst const &lhs, TypePostfixExpressionOperatorAst const &rhs) -> bool {
        return lhs.equals(rhs);
    }

protected:
    virtual auto equals(TypePostfixExpressionOperatorAst const &) const -> bool = 0;

    virtual auto equals_nested_type(TypePostfixExpressionOperatorNestedTypeAst const &) const -> bool;

    virtual auto equals_optional(TypePostfixExpressionOperatorOptionalAst const &) const -> bool;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;
};


#endif //TYPE_POSTFIX_EXPRESSION_OPERATOR_AST_HPP
