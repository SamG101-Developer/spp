#include <algorithm>

#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ConventionMutAst::ConventionMutAst(
    decltype(tok_borrow) &&tok_borrow,
    decltype(tok_mut) &&tok_mut):
    tok_borrow(std::move(tok_borrow)),
    tok_mut(std::move(tok_mut)) {
}


auto spp::asts::ConventionMutAst::pos_end() -> std::size_t {
    return tok_mut->pos_end();
}


spp::asts::ConventionMutAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_borrow);
    SPP_STRING_APPEND(tok_mut);
    SPP_STRING_END;
}


auto spp::asts::ConventionMutAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_borrow);
    SPP_PRINT_APPEND(tok_mut);
    SPP_PRINT_END;
}
