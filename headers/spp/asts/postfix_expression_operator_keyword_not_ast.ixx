module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.postfix_expression_operator_ast;

import std;


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
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

    ~PostfixExpressionOperatorKeywordNotAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
