module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionAst;
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionAst final : PrimaryExpressionAst {
private:
    std::unique_ptr<TokenAst> tok;
    std::unique_ptr<ClosureExpressionParameterAndCaptureGroupAst> pc_group;
    std::unique_ptr<ExpressionAst> body;

    explicit ClosureExpressionAst(
        decltype(tok) &&tok,
        decltype(pc_group) &&pc_group,
        decltype(body) &&body);
    ~ClosureExpressionAst() override;
    auto to_rust() const -> std::string override;
};
