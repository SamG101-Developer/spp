#include <spp/asts/let_statement_uninitialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LetStatementUninitializedAst::LetStatementUninitializedAst(
    decltype(tok_let) &&tok_let,
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type):
    LetStatementAst(),
    tok_let(std::move(tok_let)),
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::LetStatementUninitializedAst::~LetStatementUninitializedAst() = default;


auto spp::asts::LetStatementUninitializedAst::pos_start() const -> std::size_t {
    return tok_let->pos_start();
}


auto spp::asts::LetStatementUninitializedAst::pos_end() const -> std::size_t {
    return type->pos_end();
}


spp::asts::LetStatementUninitializedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_let);
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::LetStatementUninitializedAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_let);
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}
