module;
#include <spp/macros.hpp>

export module spp.asts.loop_else_statement_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LoopElseStatementAst final : Ast {
    std::unique_ptr<TokenAst> tok_else;
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    explicit LoopElseStatementAst(
        decltype(tok_else) &&tok_else,
        decltype(body) &&body);
    ~LoopElseStatementAst() override;
    auto to_rust() const -> std::string override;
};
