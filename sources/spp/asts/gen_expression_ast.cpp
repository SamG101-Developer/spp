#include <spp/asts/convention_ast.hpp>
#include <spp/asts/gen_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::GenExpressionAst::GenExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(conv) &&conv,
    decltype(expr) &&expr):
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    conv(std::move(conv)),
    expr(std::move(expr)) {
}


spp::asts::GenExpressionAst::~GenExpressionAst() = default;


auto spp::asts::GenExpressionAst::pos_start() const -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


spp::asts::GenExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_gen);
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::GenExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen);
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}
