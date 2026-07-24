module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_ast;
import spp.asts.type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    Shared<TypeUnaryExpressionOperatorAst> Op;

    /**
     * The type that is being operated on by the unary operator.
     */
    Shared<TypeAst> Rhs;

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

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto Iterator() const
        -> Vec<Shared<const TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto IsNeverType() const noexcept
        -> bool override;

    SPP_ATTR_NODISCARD auto IsSelfType() const noexcept
        -> bool override;

    SPP_ATTR_NODISCARD auto NsParts() const
        -> Vec<Shared<const IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto NsParts()
        -> Vec<Shared<IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto TypeParts() const
        -> Vec<Shared<const TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto TypeParts()
        -> Vec<Shared<TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto LastTypePart() const
        -> TypeIdentifierAst const* override;

    SPP_ATTR_NODISCARD auto LastTypePart()
        -> TypeIdentifierAst* override;

    SPP_ATTR_NODISCARD auto WithoutConvention() const
        -> Shared<const TypeAst> override;

    SPP_ATTR_NODISCARD auto GetConvention() const
        -> ConventionAst* override;

    SPP_ATTR_NODISCARD auto WithConvention(
        Unique<ConventionAst> &&conv) const
        -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto WithoutGenerics() const
        -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto SubstituteGenerics(
        Vec<GenericArgumentAst*> const &args) const
        -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto ContainsGenerics(
        GenericParameterAst const &generic) const
        -> bool override;

    SPP_ATTR_NODISCARD auto WithGenerics(
        Unique<GenericArgumentGroupAst> &&arg_group) const
        -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const
        -> bool override;

    auto ResetCache()
        -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionAst)
