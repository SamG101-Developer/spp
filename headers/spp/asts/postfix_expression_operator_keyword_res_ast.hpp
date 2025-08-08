#ifndef POSTFIX_EXPRESSION_OPERATOR_KEYWORD_RES_AST_HPP
#define POSTFIX_EXPRESSION_OPERATOR_KEYWORD_RES_AST_HPP

#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::PostfixExpressionOperatorKeywordResAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c . token that indicates a member access operation.
     */
    std::unique_ptr<TokenAst> tok_dot;

    /**
     * The @c res token that indicates a keyword res operation.
     */
    std::unique_ptr<TokenAst> tok_res;

    /**
     * The arguments passed to the res keyword. These will be passed into the mapped function for the generator type, to
     * provide uniform function call analysis.
     */
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;

    /**
     * Construct the PostfixExpressionOperatorKeywordResAst with the arguments matching the members.
     * @param tok_dot The @c . token that indicates a member access operation.
     * @param tok_res The @c res token that indicates a keyword res operation.
     * @param arg_group The arguments passed to the res keyword.
     */
    PostfixExpressionOperatorKeywordResAst(
        decltype(tok_dot) &&tok_dot,
        decltype(tok_res) &&tok_res,
        decltype(arg_group) &&arg_group);

    ~PostfixExpressionOperatorKeywordResAst() override;

private:
    std::unique_ptr<PostfixExpressionAst> m_mapped_func;
};


#endif //POSTFIX_EXPRESSION_OPERATOR_KEYWORD_RES_AST_HPP
