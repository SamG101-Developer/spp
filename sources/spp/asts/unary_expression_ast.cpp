#include <spp/asts/unary_expression_ast.hpp>
#include <spp/asts/unary_expression_operator_ast.hpp>


spp::asts::UnaryExpressionAst::UnaryExpressionAst(
    decltype(tok_op) &&tok_op,
    decltype(expr) &&expr):
    tok_op(std::move(tok_op)),
    expr(std::move(expr)) {
}


spp::asts::UnaryExpressionAst::~UnaryExpressionAst() = default;


auto spp::asts::UnaryExpressionAst::pos_start() const -> std::size_t {
    return tok_op->pos_start();
}


auto spp::asts::UnaryExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


spp::asts::UnaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}
