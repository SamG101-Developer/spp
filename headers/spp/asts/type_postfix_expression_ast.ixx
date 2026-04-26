module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
}


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The left-hand side type of the postfix expression. This is the base type on which the postfix operation is
     * applied.
     */
    std::shared_ptr<TypeAst> lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TypePostfixExpressionOperatorAst> tok_op;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side type of the postfix expression.
     * @param[in] tok_op The operator token that represents the postfix operation.
     */
    TypePostfixExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op);

    ~TypePostfixExpressionAst() override;

    auto operator<=>(
        TypePostfixExpressionAst const &other) const
        -> std::strong_ordering;

    auto operator==(
        TypePostfixExpressionAst const &other) const
        -> bool;

    auto equals_type_postfix_expression(
        TypePostfixExpressionAst const &) const
        -> std::strong_ordering override;

    auto equals(
        const ExpressionAst &) const
        -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    auto iterator() const
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


SPP_GCC_VTABLE_FIX_IMPL(TypePostfixExpressionAst)
