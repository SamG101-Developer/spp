#ifndef TYPE_POSTFIX_EXPRESSION_AST_HPP
#define TYPE_POSTFIX_EXPRESSION_AST_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left-hand side type of the postfix expression. This is the base type on which the postfix operation is
     * applied.
     */
    std::unique_ptr<TypeAst> lhs;

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

public:
    auto ns_parts() const -> std::vector<IdentifierAst*> override;

    auto type_parts() const -> std::vector<TypeIdentifierAst*> override;

    auto without_generics() const -> std::unique_ptr<AbstractTypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto substitute_generics(std::vector<GenericArgumentAst*> &&args) const -> std::unique_ptr<AbstractTypeAst> override;

    auto contains_generic() const -> bool override;

    auto set_generics() -> std::unique_ptr<AbstractTypeAst> override;

    auto with_convention() const -> std::unique_ptr<AbstractTypeAst> override;
};


#endif //TYPE_POSTFIX_EXPRESSION_AST_HPP
