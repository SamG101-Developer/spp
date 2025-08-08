#include <spp/asts/expression_ast.hpp>
#include <spp/asts/ret_statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::RetStatementAst::RetStatementAst(
    decltype(tok_ret) &&tok_ret,
    decltype(val) &&val):
    tok_ret(std::move(tok_ret)),
    val(std::move(val)) {
}


spp::asts::RetStatementAst::~RetStatementAst() = default;


auto spp::asts::RetStatementAst::pos_start() const -> std::size_t {
    return tok_ret->pos_start();
}


auto spp::asts::RetStatementAst::pos_end() const -> std::size_t {
    return val ? val->pos_end() : tok_ret->pos_end();
}


spp::asts::RetStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ret);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::RetStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ret);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
