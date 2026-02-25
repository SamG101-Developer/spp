module;
#include <spp/macros.hpp>

export module spp.asts.loop_iterable_expression_ast;
import spp.asts.loop_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
    SPP_EXP_CLS struct LoopIterableExpressionAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopIterableExpressionAst final : LoopExpressionAst {
    std::unique_ptr<LocalVariableAst> var;
    std::unique_ptr<ExpressionAst> iterable;

    explicit LoopIterableExpressionAst(
        decltype(var) &&var,
        decltype(iterable) &&iterable,
        decltype(body) &&body,
        decltype(else_block) &&else_block);
    ~LoopIterableExpressionAst() override;
    auto to_rust() const -> std::string override;
};
