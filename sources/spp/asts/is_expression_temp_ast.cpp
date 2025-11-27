module spp.asts.is_expression_temp_ast;


spp::asts::IsExpressionTempAst::IsExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionTempAst::~IsExpressionTempAst() = default;
