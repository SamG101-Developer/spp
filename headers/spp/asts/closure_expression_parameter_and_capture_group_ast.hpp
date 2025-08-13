#ifndef CLOSURE_EXPRESSION_PARAMETER_AND_CAPTURE_GROUP_AST_HPP
#define CLOSURE_EXPRESSION_PARAMETER_AND_CAPTURE_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ClosureExpressionParameterAndCaptureGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

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

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //CLOSURE_EXPRESSION_PARAMETER_AND_CAPTURE_GROUP_AST_HPP
