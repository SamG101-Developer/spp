module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_ast;
import spp.asts.mixins.compiler_stages;
import spp.asts.type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The left-hand side type of the postfix expression. This is the base type on which the postfix operation is
     * applied.
     */
    Unique<TypeAst> Lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    Unique<TypePostfixExpressionOperatorAst> TokOp;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side type of the postfix expression.
     * @param[in] tok_op The operator token that represents the postfix operation.
     */
    TypePostfixExpressionAst(
        decltype(Lhs) lhs,
        decltype(TokOp) tok_op);

    ~TypePostfixExpressionAst() override;

    auto operator<=>(TypePostfixExpressionAst const &other) const -> Ordering;
    auto operator==(TypePostfixExpressionAst const &other) const -> bool;

    SPP_ATTR_NODISCARD auto EqualsTypePostfixExpression(TypePostfixExpressionAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(const ExpressionAst &) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    SPP_ATTR_NODISCARD auto IsNeverType() const noexcept -> bool override;

    SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst*> override;

    SPP_ATTR_NODISCARD auto WithoutConvention() const -> TypeAst* override;

    SPP_ATTR_NODISCARD auto GetConvention() const -> ConventionAst* override;

    SPP_ATTR_NODISCARD auto WithConvention(Unique<ConventionAst> &&conv) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto WithoutGenerics() const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto ContainsGenerics(GenericParameterAst const &generic) const -> bool override;

    SPP_ATTR_NODISCARD auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const -> bool override;

    auto ResetCache() -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypePostfixExpressionAst)
