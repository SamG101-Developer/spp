#include <spp/asts/loop_expression_ast.hpp>
#include <spp/asts/loop_condition_ast.hpp>
#include <spp/asts/loop_else_statement_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(cond) &&cond,
    decltype(body) &&body,
    decltype(else_block) &&else_block):
    PrimaryExpressionAst(),
    tok_loop(std::move(tok_loop)),
    cond(std::move(cond)),
    body(std::move(body)),
    else_block(std::move(else_block)) {
}


spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;


auto spp::asts::LoopExpressionAst::pos_start() const -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopExpressionAst::pos_end() const -> std::size_t {
    return cond->pos_end();
}


spp::asts::LoopExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(body);
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
}
