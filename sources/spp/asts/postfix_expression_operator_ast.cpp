module spp.asts;


spp::asts::PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;


spp::asts::PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;


auto spp::asts::PostfixExpressionOperatorAst::expr_parts() const
    -> std::vector<Ast*> {
    return {};
}
