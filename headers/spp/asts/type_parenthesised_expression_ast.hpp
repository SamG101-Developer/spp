#ifndef TYPE_PARENTHESISED_EXPRESSION_AST_HPP
#define TYPE_PARENTHESISED_EXPRESSION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/temp_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeParenthesisedExpressionAst final : virtual Ast, mixins::TempTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left parenthesis token that represents the start of the parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The type expression that is enclosed within the parentheses.
     */
    std::unique_ptr<TypeAst> expr;

    /**
     * The right parenthesis token that represents the end of the parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the TypeParenthesisedExpressionAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param expr The type expression within the parentheses.
     * @param tok_r The right parenthesis token.
     */
    TypeParenthesisedExpressionAst(
        decltype(tok_l) &&tok_l,
        decltype(expr) &&expr,
        decltype(tok_r) &&tok_r);

    ~TypeParenthesisedExpressionAst() override;

    auto convert() -> std::unique_ptr<TypeAst> override;
};


#endif //TYPE_PARENTHESISED_EXPRESSION_AST_HPP
