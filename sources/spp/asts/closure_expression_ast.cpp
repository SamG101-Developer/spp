#include <spp/asts/closure_expression_ast.hpp>
#include <spp/asts/closure_expression_parameter_and_capture_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ClosureExpressionAst::ClosureExpressionAst(
    decltype(tok_cor) &&tok_cor,
    decltype(pc_group) &&pc_group,
    decltype(body) &&body):
    PrimaryExpressionAst(),
    tok_cor(std::move(tok_cor)),
    pc_group(std::move(pc_group)),
    body(std::move(body)) {
}


spp::asts::ClosureExpressionAst::~ClosureExpressionAst() = default;


auto spp::asts::ClosureExpressionAst::pos_start() const -> std::size_t {
    return tok_cor ? tok_cor->pos_start() : pc_group->pos_start();
}


auto spp::asts::ClosureExpressionAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


spp::asts::ClosureExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_cor);
    SPP_STRING_APPEND(pc_group);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_cor);
    SPP_PRINT_APPEND(pc_group);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}
