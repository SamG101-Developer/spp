module;
#include <spp/macros.hpp>

export module spp.asts.loop_control_flow_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct LoopControlFlowStatementAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LoopControlFlowStatementAst final : StatementAst {
    std::vector<std::unique_ptr<TokenAst>> tok_seq_exit;
    std::unique_ptr<TokenAst> tok_skip;
    std::unique_ptr<ExpressionAst> expr;

    explicit LoopControlFlowStatementAst(
        decltype(tok_seq_exit) &&tok_seq_exit,
        decltype(tok_skip) &&tok_skip,
        decltype(expr) &&expr);
    ~LoopControlFlowStatementAst() override;
    auto to_rust() const -> std::string override;
};
