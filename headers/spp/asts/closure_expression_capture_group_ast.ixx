module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureGroupAst final : Ast {
    std::unique_ptr<TokenAst> tok_caps;
    std::vector<std::unique_ptr<ClosureExpressionCaptureAst>> captures;

    explicit ClosureExpressionCaptureGroupAst(
        decltype(tok_caps) &&tok_caps,
        decltype(captures) &&captures);
    ~ClosureExpressionCaptureGroupAst() override;
    auto to_rust() const -> std::string override;
};
