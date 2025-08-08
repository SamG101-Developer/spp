#include <algorithm>

#include <spp/asts/array_literal_explicit_elements_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ArrayLiteralExplicitElements::ArrayLiteralExplicitElements(
    decltype(tok_l) &&tok_l,
    decltype(elements) &&elements,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    elements(std::move(elements)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::ArrayLiteralExplicitElements::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ArrayLiteralExplicitElements::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ArrayLiteralExplicitElements::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elements);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralExplicitElements::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elements);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
