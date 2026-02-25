module;
#include <spp/macros.hpp>

export module spp.asts.loop_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LoopExpressionAst : PrimaryExpressionAst {
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;
    std::unique_ptr<LoopElseStatementAst> else_block;

    explicit LoopExpressionAst(
        decltype(body) &&body,
        decltype(else_block) &&else_block);
    ~LoopExpressionAst() override;
    auto to_rust() const -> std::string override;
};
