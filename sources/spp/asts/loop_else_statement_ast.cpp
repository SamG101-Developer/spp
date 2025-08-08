#include <spp/asts/loop_else_statement_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LoopElseStatementAst::LoopElseStatementAst(
    decltype(tok_else) &&tok_else,
    decltype(body) &&body):
    tok_else(std::move(tok_else)),
    body(std::move(body)) {
}


spp::asts::LoopElseStatementAst::~LoopElseStatementAst() = default;


auto spp::asts::LoopElseStatementAst::pos_start() const -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::LoopElseStatementAst::pos_end() const -> std::size_t {
    return tok_else->pos_end();
}


spp::asts::LoopElseStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::LoopElseStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}
