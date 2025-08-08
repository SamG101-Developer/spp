#include <spp/asts/gen_with_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::GenWithExpressionAst::GenWithExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(tok_with) &&tok_with,
    decltype(expr) &&expr):
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    tok_with(std::move(tok_with)),
    expr(std::move(expr)) {
}


spp::asts::GenWithExpressionAst::~GenWithExpressionAst() = default;


auto spp::asts::GenWithExpressionAst::pos_start() const -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenWithExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


spp::asts::GenWithExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_gen);
    SPP_STRING_APPEND(tok_with);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::GenWithExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen);
    SPP_PRINT_APPEND(tok_with);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}
