#ifndef TYPE_POSTFIX_EXPRESSION_OPERATOR_NESTED_TYPE_AST_HPP
#define TYPE_POSTFIX_EXPRESSION_OPERATOR_NESTED_TYPE_AST_HPP

#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypePostfixExpressionOperatorNestedTypeAst final : TypePostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c :: operator token that represents the namespace operator.
     */
    std::unique_ptr<TokenAst> tok_sep;

    /**
     * The nested type identifier. This is the type that is being accessed within the outer type.
     */
    std::unique_ptr<TypeIdentifierAst> name;

    /**
     * Construct the TypePostfixExpressionOperatorNestedTypeAst with the arguments matching the members.
     * @param tok_sep The @c :: operator token that represents the namespace operator.
     * @param name The nested type identifier.
     */
    TypePostfixExpressionOperatorNestedTypeAst(
        decltype(tok_sep) &&tok_sep,
        decltype(name) &&name);

    ~TypePostfixExpressionOperatorNestedTypeAst() override;

    auto ns_parts() const -> std::vector<IdentifierAst const*> override;

    auto type_parts() const -> std::vector<TypeIdentifierAst const*> override;
};


#endif //TYPE_POSTFIX_EXPRESSION_OPERATOR_NESTED_TYPE_AST_HPP
