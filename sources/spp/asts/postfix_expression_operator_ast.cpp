module spp.asts.postfix_expression_operator_ast;


spp::asts::PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;


spp::asts::PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;


auto spp::asts::PostfixExpressionOperatorAst::expr_parts() const
    -> std::vector<Ast*> {
    return {};
}
