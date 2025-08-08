#include <spp/asts/postfix_expression_operator_early_return_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
    decltype(tok_qst) &&tok_qst) :
    PostfixExpressionOperatorAst(),
    tok_qst(std::move(tok_qst)) {
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_start() const -> std::size_t {
    return tok_qst->pos_start();
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_end() const -> std::size_t {
    return tok_qst->pos_end();
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_qst);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_qst);
    SPP_PRINT_END;
}
