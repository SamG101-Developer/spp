#include <spp/asts/parenthesised_expression.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ParenthesisedExpressionAst::ParenthesisedExpressionAst(
    decltype(tok_open_paren) &&tok_open_paren,
    decltype(expr) &&expr,
    decltype(tok_close_paren) &&tok_close_paren):
    PrimaryExpressionAst(),
    tok_open_paren(std::move(tok_open_paren)),
    expr(std::move(expr)),
    tok_close_paren(std::move(tok_close_paren)) {
}


spp::asts::ParenthesisedExpressionAst::~ParenthesisedExpressionAst() = default;


auto spp::asts::ParenthesisedExpressionAst::pos_start() const -> std::size_t {
    return tok_open_paren->pos_start();
}


auto spp::asts::ParenthesisedExpressionAst::pos_end() const -> std::size_t {
    return tok_close_paren->pos_end();
}


spp::asts::ParenthesisedExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_open_paren);
    SPP_STRING_APPEND(expr);
    SPP_STRING_APPEND(tok_close_paren);
    SPP_STRING_END;
}


auto spp::asts::ParenthesisedExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_open_paren);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_APPEND(tok_close_paren);
    SPP_PRINT_END;
}
