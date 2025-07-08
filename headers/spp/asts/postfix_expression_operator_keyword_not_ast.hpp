#ifndef POSTFIX_EXPRESSION_OPERATOR_KEYWORD_NOT_AST_HPP
#define POSTFIX_EXPRESSION_OPERATOR_KEYWORD_NOT_AST_HPP

#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c . token that indicates a member access operation.
     */
    std::unique_ptr<TokenAst> tok_dot;

    /**
     * The @c not token that indicates a keyword not operation.
     */
    std::unique_ptr<TokenAst> tok_not;

    /**
     * Construct the PostfixExpressionOperatorKeywordNotAst with the arguments matching the members.
     * @param tok_dot The @c . token that indicates a member access operation.
     * @param tok_not The @c not token that indicates a keyword not operation.
     */
    PostfixExpressionOperatorKeywordNotAst(
        decltype(tok_dot) &&tok_dot,
        decltype(tok_not) &&tok_not);
};


#endif //POSTFIX_EXPRESSION_OPERATOR_KEYWORD_NOT_AST_HPP
