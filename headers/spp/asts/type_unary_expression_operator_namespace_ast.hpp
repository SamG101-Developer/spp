#ifndef TYPE_UNARY_EXPRESSION_OPERATOR_NAMESPACE_AST_HPP
#define TYPE_UNARY_EXPRESSION_OPERATOR_NAMESPACE_AST_HPP

#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionOperatorNamespaceAst final : TypeUnaryExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The namespace token that represents the namespace in which the type is defined.
     */
    std::shared_ptr<IdentifierAst> ns;

    /**
     * The @c :: operator token that represents the namespace operator.
     */
    std::unique_ptr<TokenAst> tok_sep;

    /**
     * Construct the TypeUnaryExpressionOperatorNamespaceAst with the arguments matching the members.
     * @param[in] ns The namespace token that represents the namespace in which the type is defined.
     * @param[in] tok_sep The @c :: operator token that represents the namespace operator.
     */
    explicit TypeUnaryExpressionOperatorNamespaceAst(
        decltype(ns) ns,
        decltype(tok_sep) &&tok_sep);

    ~TypeUnaryExpressionOperatorNamespaceAst() override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_NAMESPACE_AST_HPP
