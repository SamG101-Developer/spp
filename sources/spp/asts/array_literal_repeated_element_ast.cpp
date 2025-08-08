#include <spp/asts/array_literal_repeated_element_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ArrayLiteralRepeatedElementAst::ArrayLiteralRepeatedElementAst(
        decltype(tok_l) &&tok_l,
    decltype(elem) &&elem,
    decltype(tok_semicolon) &&tok_semicolon,
    decltype(size) &&size,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    elem(std::move(elem)),
    tok_semicolon(std::move(tok_semicolon)),
    size(std::move(size)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::ArrayLiteralRepeatedElementAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ArrayLiteralRepeatedElementAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ArrayLiteralRepeatedElementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(elem);
    SPP_STRING_APPEND(tok_semicolon);
    SPP_STRING_APPEND(size);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralRepeatedElementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(elem);
    SPP_PRINT_APPEND(tok_semicolon);
    SPP_PRINT_APPEND(size);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
