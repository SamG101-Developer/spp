module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_ast;


SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;


spp::asts::PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;


auto spp::asts::PostfixExpressionOperatorAst::expr_parts() const
    -> std::vector<Ast*> {
    return {};
}

SPP_MOD_END
