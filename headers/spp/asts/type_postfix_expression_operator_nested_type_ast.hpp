#pragma once
#include <spp/asts/type_postfix_expression_operator_ast.hpp>


struct spp::asts::TypePostfixExpressionOperatorNestedTypeAst final : TypePostfixExpressionOperatorAst {
    /**
     * The @c :: operator token that represents the namespace operator.
     */
    std::unique_ptr<TokenAst> tok_sep;

    /**
     * The nested type identifier. This is the type that is being accessed within the outer type.
     */
    std::shared_ptr<TypeIdentifierAst> name;

    /**
     * Construct the TypePostfixExpressionOperatorNestedTypeAst with the arguments matching the members.
     * @param tok_sep The @c :: operator token that represents the namespace operator.
     * @param name The nested type identifier.
     */
    TypePostfixExpressionOperatorNestedTypeAst(
        decltype(tok_sep) &&tok_sep,
        decltype(name) &&name);

    ~TypePostfixExpressionOperatorNestedTypeAst() override;

protected:
    auto equals(TypePostfixExpressionOperatorAst const &) const -> std::strong_ordering override;

    auto equals_nested_type(TypePostfixExpressionOperatorNestedTypeAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;
};
