module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_ast;
import spp.asts.type_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    Unique<TypeUnaryExpressionOperatorAst> Op;

    /**
     * The type that is being operated on by the unary operator.
     */
    Unique<TypeAst> Rhs;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] op The operator token that represents the unary operation.
     * @param[in] rhs The type that is being operated on by the unary operator.
     */
    TypeUnaryExpressionAst(
        decltype(Op) op,
        decltype(Rhs) rhs);

    ~TypeUnaryExpressionAst() override;

    auto operator<=>(TypeUnaryExpressionAst const &other) const -> Ordering;
    auto operator==(TypeUnaryExpressionAst const &other) const -> bool;

    SPP_ATTR_NODISCARD auto EqualsTypeUnaryExpression(TypeUnaryExpressionAst const &other) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    SPP_ATTR_NODISCARD auto IsNeverType() const noexcept -> bool override;

    SPP_ATTR_NODISCARD auto IsSelfType() const noexcept -> bool override;

    SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst*> override;

    SPP_ATTR_NODISCARD auto WithoutConvention() const -> TypeAst* override;

    SPP_ATTR_NODISCARD auto GetConvention() const -> ConventionAst* override;

    SPP_ATTR_NODISCARD auto WithConvention(Unique<ConventionAst> &&conv) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto WithoutGenerics() const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto SubstituteGenerics(
        Vec<GenericArgumentAst*> const &args) const
        -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto ContainsGenerics(
        GenericParameterAst const &generic) const
        -> bool override;

    SPP_ATTR_NODISCARD auto WithGenerics(
        Unique<GenericArgumentGroupAst> &&arg_group) const
        -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const
        -> bool override;

    auto ResetCache()
        -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionAst)
