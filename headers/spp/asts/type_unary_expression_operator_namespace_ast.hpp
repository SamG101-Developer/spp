#pragma once
#include <spp/asts/type_unary_expression_operator_ast.hpp>


struct spp::asts::TypeUnaryExpressionOperatorNamespaceAst final : TypeUnaryExpressionOperatorAst {
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

protected:
    auto equals(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering override;

    auto equals_op_namespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;
};
