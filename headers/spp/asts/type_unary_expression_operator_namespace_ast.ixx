module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorNamespaceAst final : TypeUnaryExpressionOperatorAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The namespace token that represents the namespace in which the type is defined.
     */
    Shared<IdentifierAst> Ns;

    /**
     * The @c :: operator token that represents the namespace operator.
     */
    Unique<TokenAst> TokSep;

    /**
     * Construct the TypeUnaryExpressionOperatorNamespaceAst with the arguments matching the members.
     * @param[in] ns The namespace token that represents the namespace in which the type is defined.
     * @param[in] tok_sep The @c :: operator token that represents the namespace operator.
     */
    explicit TypeUnaryExpressionOperatorNamespaceAst(
        decltype(Ns) ns,
        decltype(TokSep) &&tok_sep);

    ~TypeUnaryExpressionOperatorNamespaceAst() override;

    SPP_ATTR_NODISCARD auto EqualsOpNamespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(TypeUnaryExpressionOperatorAst const &) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS

    SPP_ATTR_NODISCARD auto NsParts() const
        -> Vec<Shared<const IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto NsParts()
        -> Vec<Shared<IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto TypeParts() const
        -> Vec<Shared<const TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto TypeParts()
        -> Vec<Shared<TypeIdentifierAst>> override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionOperatorNamespaceAst)
