module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.token_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorNestedTypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorNestedTypeAst final : TypePostfixExpressionOperatorAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The @c :: operator token that represents the namespace operator.
     */
    Unique<TokenAst> TokSep;

    /**
     * The nested type identifier. This is the type that is being accessed within the outer type.
     */
    Unique<TypeIdentifierAst> Name;

    /**
     * Construct the TypePostfixExpressionOperatorNestedTypeAst with the arguments matching the members.
     * @param tok_sep The @c :: operator token that represents the namespace operator.
     * @param name The nested type identifier.
     */
    TypePostfixExpressionOperatorNestedTypeAst(
        decltype(TokSep) &&tok_sep,
        decltype(Name) &&name);

    ~TypePostfixExpressionOperatorNestedTypeAst() override;

    SPP_ATTR_NODISCARD auto EqualsNestedType(TypePostfixExpressionOperatorNestedTypeAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(TypePostfixExpressionOperatorAst const &) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypePostfixExpressionOperatorNestedTypeAst)
