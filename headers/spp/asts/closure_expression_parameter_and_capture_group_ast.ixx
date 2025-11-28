module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts._fwd;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionParameterAndCaptureGroupAst final : virtual Ast {
    /**
     * The @c | token that indicates the start of the closure parameter and capture group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The parameters of the closure. This is a list of parameters that will be passed to the closure when it is called.
     */
    std::unique_ptr<ClosureExpressionParameterGroupAst> param_group;

    /**
     * The captured variables from the outer scope. These are variables that are captured by the closure and can be used
     * within its body.
     */
    std::unique_ptr<ClosureExpressionCaptureGroupAst> capture_group;

    /**
     * The @c | token that indicates the end of the closure parameter and capture group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ClosureExpressionParameterAndCaptureGroupAst with the arguments matching the members.
     * @param tok_l The @c | token that indicates the start of the closure parameter and capture group.
     * @param param_group The parameters of the closure.
     * @param capture_group The captured variables from the outer scope.
     * @param tok_r The @c | token that indicates the end of the closure parameter and capture group.
     */
    ClosureExpressionParameterAndCaptureGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(param_group) &&param_group,
        decltype(capture_group) &&capture_group,
        decltype(tok_r) &&tok_r);

    ~ClosureExpressionParameterAndCaptureGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
