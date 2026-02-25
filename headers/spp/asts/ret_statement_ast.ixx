module;
#include <spp/macros.hpp>

export module spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct RetStatementAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::RetStatementAst final : StatementAst {
    std::unique_ptr<TokenAst> tok_ret;
    std::unique_ptr<ExpressionAst> expr;

    RetStatementAst(
        decltype(tok_ret) &&tok_ret,
        decltype(expr) &&val);
    ~RetStatementAst() override;
    auto to_rust() const -> std::string override;
};
