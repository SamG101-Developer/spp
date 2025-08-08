#include <spp/asts/unary_expression_operator_deref_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::UnaryExpressionOperatorDerefAst::UnaryExpressionOperatorDerefAst(
    decltype(tok_deref) &&tok_deref):
    tok_deref(std::move(tok_deref)) {
}


spp::asts::UnaryExpressionOperatorDerefAst::~UnaryExpressionOperatorDerefAst() = default;


auto spp::asts::UnaryExpressionOperatorDerefAst::pos_start() const -> std::size_t {
    return tok_deref->pos_start();
}


auto spp::asts::UnaryExpressionOperatorDerefAst::pos_end() const -> std::size_t {
    return tok_deref->pos_end();
}


spp::asts::UnaryExpressionOperatorDerefAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_deref);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorDerefAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_deref);
    SPP_PRINT_END;
}
