#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PatternGuardAst::PatternGuardAst(
    decltype(tok_and) &&tok_and,
    decltype(expression) &&expression):
    tok_and(std::move(tok_and)),
    expression(std::move(expression)) {
}


auto spp::asts::PatternGuardAst::pos_start() const -> std::size_t {
    return tok_and->pos_start();
}


auto spp::asts::PatternGuardAst::pos_end() const -> std::size_t {
    return expression->pos_end();
}


spp::asts::PatternGuardAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_and);
    SPP_STRING_APPEND(expression);
    SPP_STRING_END;
}


auto spp::asts::PatternGuardAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_and);
    SPP_PRINT_APPEND(expression);
    SPP_PRINT_END;
}
