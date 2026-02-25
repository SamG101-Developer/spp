module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorEarlyReturnAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
    explicit PostfixExpressionOperatorEarlyReturnAst();
    ~PostfixExpressionOperatorEarlyReturnAst() override;
    auto to_rust() const -> std::string override;
};
