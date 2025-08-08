#ifndef TYPE_UNARY_EXPRESSION_AST_HPP
#define TYPE_UNARY_EXPRESSION_AST_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TypeUnaryExpressionOperatorAst> op;

    /**
     * The type that is being operated on by the unary operator.
     */
    std::unique_ptr<TypeAst> rhs;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] op The operator token that represents the unary operation.
     * @param[in] rhs The type that is being operated on by the unary operator.
     */
    TypeUnaryExpressionAst(
        decltype(op) &&op,
        decltype(rhs) &&rhs);

public:
    auto is_never_type() const -> bool override;

    auto ns_parts() const -> std::vector<IdentifierAst const*> override;

    auto type_parts() const -> std::vector<TypeIdentifierAst const*> override;

    auto without_convention() const -> TypeAst const* override;

    auto get_convention() const -> ConventionAst* override;

    auto with_convention(std::unique_ptr<ConventionAst> &&convention) const -> std::unique_ptr<TypeAst> override;

    auto without_generics() const -> std::unique_ptr<TypeAst> override;

    auto substitute_generics(std::vector<GenericArgumentAst*> args) const -> std::unique_ptr<TypeAst> override;

    auto contains_generic(TypeAst const *generic) const -> bool override;

    auto match_generic(TypeAst const *other, TypeAst const *generic) -> TypeAst* override;

    auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) -> std::unique_ptr<TypeAst> override;
};


#endif //TYPE_UNARY_EXPRESSION_AST_HPP
