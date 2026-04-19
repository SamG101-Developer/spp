module spp.asts;


spp::asts::BinaryExpressionTempAst::BinaryExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::BinaryExpressionTempAst::~BinaryExpressionTempAst() = default;
