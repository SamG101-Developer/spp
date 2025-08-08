#ifndef POSTFIX_EXPRESSION_OPERATOR_EARLY_RETURN_AST_HPP
#define POSTFIX_EXPRESSION_OPERATOR_EARLY_RETURN_AST_HPP

#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c ? token that indicates an early return in a postfix expression. This token is used to signify that the
     * expression should be checked for its result-type failure type, and if it matches, the expression will lift the
     * error to the caller.
     */
    std::unique_ptr<TokenAst> tok_qst;

    /**
     * Construct the PostfixExpressionOperatorEarlyReturnAst with the arguments matching the members.
     * @param[in] tok_qst The @c ? token that indicates an early return in a postfix expression.
     */
    explicit PostfixExpressionOperatorEarlyReturnAst(
        decltype(tok_qst)&& tok_qst);

    ~PostfixExpressionOperatorEarlyReturnAst() override;
};


#endif //POSTFIX_EXPRESSION_OPERATOR_EARLY_RETURN_AST_HPP
