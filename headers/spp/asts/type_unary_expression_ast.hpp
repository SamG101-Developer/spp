#ifndef TYPE_UNARY_EXPRESSION_AST_HPP
#define TYPE_UNARY_EXPRESSION_AST_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

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

protected:
    auto equals(ExpressionAst const &other) const -> bool override;

    auto equals_type_unary_expression(TypeUnaryExpressionAst const &other) const -> bool override;

public:
    auto iterator() const -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> override;

    auto is_never_type() const -> bool override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;

    auto without_convention() const -> std::shared_ptr<const TypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::unique_ptr<TypeAst> override;

    auto without_generics() const -> std::unique_ptr<TypeAst> override;

    auto substitute_generics(std::vector<GenericArgumentAst*> args) const -> std::unique_ptr<TypeAst> override;

    auto contains_generic(GenericParameterAst const &generic) const -> bool override;

    auto match_generic(TypeAst const &other, TypeIdentifierAst const &generic_name) const -> const ExpressionAst* override;

    auto with_generics(std::shared_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::unique_ptr<TypeAst> override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //TYPE_UNARY_EXPRESSION_AST_HPP
