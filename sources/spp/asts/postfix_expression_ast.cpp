#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>


spp::asts::PostfixExpressionAst::PostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op):
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)) {
}


auto spp::asts::PostfixExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::PostfixExpressionAst::pos_end() const -> std::size_t {
    return tok_op->pos_end();
}


spp::asts::PostfixExpressionAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_END;
}
