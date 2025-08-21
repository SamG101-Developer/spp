#ifndef UNARY_EXPRESSION_OPERATOR_DEREF_AST_HPP
#define UNARY_EXPRESSION_OPERATOR_DEREF_AST_HPP

#include <spp/asts/unary_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::UnaryExpressionOperatorDerefAst final : UnaryExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c * token that indicates a dereference operation. This is used to extract a copyable value from a borrow.
     */
    std::unique_ptr<TokenAst> tok_deref;

    /**
     * Construct the UnaryExpressionOperatorDerefAst with the arguments matching the members.
     * @param tok_deref The @c * token that indicates a dereference operation.
     */
    explicit UnaryExpressionOperatorDerefAst(
        decltype(tok_deref) &&tok_deref);

    ~UnaryExpressionOperatorDerefAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //UNARY_EXPRESSION_OPERATOR_DEREF_AST_HPP
