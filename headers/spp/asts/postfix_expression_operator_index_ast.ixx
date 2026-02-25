module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_index_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorIndexAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorIndexAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<TokenAst> tok_mut;
    std::unique_ptr<ExpressionAst> expr;

    explicit PostfixExpressionOperatorIndexAst(
        decltype(tok_mut) &&tok_mut,
        decltype(expr) &&expr);
    ~PostfixExpressionOperatorIndexAst() override;
    auto to_rust() const -> std::string override;
};
