module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordNotAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<TokenAst> tok_dot;
    std::unique_ptr<TokenAst> tok_not;

    PostfixExpressionOperatorKeywordNotAst(
        decltype(tok_dot) &&tok_dot,
        decltype(tok_not) &&tok_not);
    ~PostfixExpressionOperatorKeywordNotAst() override;
    auto to_rust() const -> std::string override;
};
