#ifndef PARENTHESISED_EXPRESSION_HPP
#define PARENTHESISED_EXPRESSION_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c ( token that indicates the start of a parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_open_paren;

    /**
     * The expression that is enclosed in parentheses.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * The @c ) token that indicates the end of a parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_close_paren;

    /**
     * Construct the ParenthesisedExpressionAst with the arguments matching the members.
     * @param[in] tok_open_paren The @c ( token that indicates the start of a parenthesised expression.
     * @param[in] expr The expression that is enclosed in parentheses.
     * @param[in] tok_close_paren The @c ) token that indicates the end of a parenthesised expression.
     */
    explicit ParenthesisedExpressionAst(
        decltype(tok_open_paren) &&tok_open_paren,
        decltype(expr) &&expr,
        decltype(tok_close_paren) &&tok_close_paren);

    ~ParenthesisedExpressionAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //PARENTHESISED_EXPRESSION_HPP
