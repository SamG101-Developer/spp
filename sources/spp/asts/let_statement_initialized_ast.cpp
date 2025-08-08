#include <spp/asts/expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LetStatementInitializedAst::LetStatementInitializedAst(
    decltype(tok_let) &&tok_let,
    decltype(var) &&var,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val):
    tok_let(std::move(tok_let)),
    var(std::move(var)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
}


auto spp::asts::LetStatementInitializedAst::pos_start() const -> std::size_t {
    return tok_let->pos_start();
}


auto spp::asts::LetStatementInitializedAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::LetStatementInitializedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_let);
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::LetStatementInitializedAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_let);
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
