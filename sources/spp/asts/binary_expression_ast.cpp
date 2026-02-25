module spp.asts.binary_expression_ast;
import spp.asts.token_ast;


spp::asts::BinaryExpressionAst::BinaryExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;


auto spp::asts::BinaryExpressionAst::to_rust() const
    -> std::string {
    return std::format("({} {} {})", lhs->to_rust(), tok_op->to_rust(), rhs->to_rust());
}
