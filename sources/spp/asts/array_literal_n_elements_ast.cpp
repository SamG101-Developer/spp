#include <algorithm>

#include <spp/asts/array_literal_n_elements_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ArrayLiteralNElements::ArrayLiteralNElements(
        decltype(tok_l) &&tok_l,
    decltype(elements) &&elements,
    decltype(tok_r) &&tok_r):
    LiteralAst(pos),
    tok_l(std::move(tok_l)),
    elements(std::move(elements)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::ArrayLiteralNElements::pos_end() -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ArrayLiteralNElements::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elements);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralNElements::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elements);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
