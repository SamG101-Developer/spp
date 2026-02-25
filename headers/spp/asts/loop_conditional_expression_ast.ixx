module;
#include <spp/macros.hpp>

export module spp.asts.loop_conditional_expression_ast;
import spp.asts.loop_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
}


SPP_EXP_CLS struct spp::asts::LoopConditionalExpressionAst final : LoopExpressionAst {
    std::unique_ptr<ExpressionAst> cond;

    explicit LoopConditionalExpressionAst(
        decltype(cond) &&cond,
        decltype(body) &&body,
        decltype(else_block) &&else_block);
    ~LoopConditionalExpressionAst() override;
    auto to_rust() const -> std::string override;
};
