#include <spp/asts/postfix_expression_operator_keyword_not_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PostfixExpressionOperatorKeywordNotAst::PostfixExpressionOperatorKeywordNotAst(
    decltype(tok_dot) &&tok_dot,
    decltype(tok_not) &&tok_not):
    PostfixExpressionOperatorAst(),
    tok_dot(std::move(tok_dot)),
    tok_not(std::move(tok_not)) {
}


spp::asts::PostfixExpressionOperatorKeywordNotAst::~PostfixExpressionOperatorKeywordNotAst() = default;


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::pos_start() const -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::pos_end() const -> std::size_t {
    return tok_not->pos_end();
}


spp::asts::PostfixExpressionOperatorKeywordNotAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(tok_not);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(tok_not);
    SPP_PRINT_END;
}
