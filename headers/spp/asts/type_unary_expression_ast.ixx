module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_ast;
import spp.asts.type_ast;
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
    std::shared_ptr<TypeUnaryExpressionOperatorAst> op;

    /**
     * The type that is being operated on by the unary operator.
     */
    std::shared_ptr<TypeAst> rhs;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] op The operator token that represents the unary operation.
     * @param[in] rhs The type that is being operated on by the unary operator.
     */
    TypeUnaryExpressionAst(
        decltype(op) op,
        decltype(rhs) rhs);

    ~TypeUnaryExpressionAst() override;

    auto operator<=>(
        TypeUnaryExpressionAst const &other) const
        -> std::strong_ordering;

    auto operator==(
        TypeUnaryExpressionAst const &other) const
        -> bool;

    auto equals_type_unary_expression(
        TypeUnaryExpressionAst const &other) const
        -> std::strong_ordering override;

    auto equals(
        ExpressionAst const &other) const
        -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    SPP_ATTR_NODISCARD auto iterator() const
        -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto is_never_type() const
        -> bool override;

    SPP_ATTR_NODISCARD auto ns_parts() const
        -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto ns_parts()
        -> std::vector<std::shared_ptr<IdentifierAst>> override;

    SPP_ATTR_NODISCARD auto type_parts() const
        -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto type_parts()
        -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;

    SPP_ATTR_NODISCARD auto without_convention() const
        -> std::shared_ptr<const TypeAst> override;

    SPP_ATTR_NODISCARD auto get_convention() const
        -> ConventionAst* override;

    SPP_ATTR_NODISCARD auto with_convention(
        std::unique_ptr<ConventionAst> &&conv) const
        -> std::shared_ptr<TypeAst> override;

    SPP_ATTR_NODISCARD auto without_generics() const
        -> std::shared_ptr<TypeAst> override;

    SPP_ATTR_NODISCARD auto substitute_generics(
        std::vector<GenericArgumentAst*> const &args) const
        -> std::shared_ptr<TypeAst> override;

    SPP_ATTR_NODISCARD auto contains_generic(
        GenericParameterAst const &generic) const
        -> bool override;

    SPP_ATTR_NODISCARD auto with_generics(
        std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const
        -> std::shared_ptr<TypeAst> override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionAst)
