#include <algorithm>

#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LoopControlFlowStatementAst::LoopControlFlowStatementAst(
    decltype(tok_seq_exit) &&tok_seq_exit,
    decltype(tok_skip) &&tok_skip,
    decltype(expr) &&expr):
    StatementAst(),
    tok_seq_exit(std::move(tok_seq_exit)),
    tok_skip(std::move(tok_skip)),
    expr(std::move(expr)) {
}


spp::asts::LoopControlFlowStatementAst::~LoopControlFlowStatementAst() = default;


auto spp::asts::LoopControlFlowStatementAst::pos_start() const -> std::size_t {
    return tok_seq_exit.empty() ? tok_skip->pos_start() : tok_seq_exit.front()->pos_start();
}


auto spp::asts::LoopControlFlowStatementAst::pos_end() const -> std::size_t {
    return expr ? expr->pos_end() : tok_skip ? tok_skip->pos_end() : tok_seq_exit.back()->pos_end();
}


spp::asts::LoopControlFlowStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(tok_seq_exit);
    SPP_STRING_APPEND(tok_skip);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::LoopControlFlowStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(tok_seq_exit);
    SPP_PRINT_APPEND(tok_skip);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}
