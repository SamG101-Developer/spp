module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionParameterAndCaptureGroupAst final : Ast {
    std::unique_ptr<ClosureExpressionParameterGroupAst> param_group;
    std::unique_ptr<ClosureExpressionCaptureGroupAst> capture_group;

    explicit ClosureExpressionParameterAndCaptureGroupAst(
        decltype(param_group) &&param_group,
        decltype(capture_group) &&capture_group);
    ~ClosureExpressionParameterAndCaptureGroupAst() override;
    auto to_rust() const -> std::string override;
};
