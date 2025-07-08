#include <spp/asts/array_literal_0_elements.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ArrayLiteral0Elements::ArrayLiteral0Elements(
        decltype(tok_l) &&tok_l,
    decltype(elem_type) &&elem_type,
    decltype(tok_comma) &&tok_comma,
    decltype(size) &&size,
    decltype(tok_r) &&tok_r):
    LiteralAst(pos),
    tok_l(std::move(tok_l)),
    elem_type(std::move(elem_type)),
    tok_comma(std::move(tok_comma)),
    size(std::move(size)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::ArrayLiteral0Elements::pos_end() -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ArrayLiteral0Elements::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(elem_type);
    SPP_STRING_APPEND(tok_comma);
    SPP_STRING_APPEND(size);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteral0Elements::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(elem_type);
    SPP_PRINT_APPEND(tok_comma);
    SPP_PRINT_APPEND(size);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
