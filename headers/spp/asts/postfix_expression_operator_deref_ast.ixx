module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorDerefAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorDerefAst final : PostfixExpressionOperatorAst {
    explicit PostfixExpressionOperatorDerefAst();
    ~PostfixExpressionOperatorDerefAst() override;
    auto to_rust() const -> std::string override;
};
