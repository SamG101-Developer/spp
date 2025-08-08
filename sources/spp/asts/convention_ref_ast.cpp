#include <algorithm>

#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ConventionRefAst::ConventionRefAst(
    decltype(tok_borrow) &&tok_borrow):
    tok_borrow(std::move(tok_borrow)) {
}


auto spp::asts::ConventionRefAst::pos_start() const -> std::size_t {
    return tok_borrow->pos_start();
}


auto spp::asts::ConventionRefAst::pos_end() const -> std::size_t {
    return tok_borrow->pos_end();
}


spp::asts::ConventionRefAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_borrow);
    SPP_STRING_END;
}


auto spp::asts::ConventionRefAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_borrow);
    SPP_PRINT_END;
}
