#pragma once
#include <spp/asts/type_ast.hpp>


struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

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

    auto operator<=>(TypePostfixExpressionAst const &) const -> std::strong_ordering;

    auto operator==(TypePostfixExpressionAst const &) const -> bool;

protected:
    auto equals(const ExpressionAst &) const -> std::strong_ordering override;

    auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> std::strong_ordering override;

public:
    SPP_NO_ASAN
    auto iterator() const -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> override;

    auto is_never_type() const -> bool override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;

    auto without_convention() const -> std::shared_ptr<const TypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::shared_ptr<TypeAst> override;

    auto without_generics() const -> std::shared_ptr<TypeAst> override;

    auto substitute_generics(std::vector<GenericArgumentAst*> const &args) const -> std::shared_ptr<TypeAst> override;

    auto contains_generic(GenericParameterAst const &generic) const -> bool override;

    auto match_generic(TypeAst const &other, TypeIdentifierAst const &generic_name) const -> const ExpressionAst* override;

    auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::shared_ptr<TypeAst> override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
