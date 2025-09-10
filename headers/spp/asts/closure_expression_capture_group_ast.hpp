#pragma once
#include <spp/asts/ast.hpp>

struct spp::asts::ClosureExpressionCaptureGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c caps token that indicates the start of the closure capture group. This is used to indicate that the
     * closure has moved on from parameter definitions and is now capturing variables from the outer scope.
     */
    std::unique_ptr<TokenAst> tok_caps;

    /**
     * The captured variables from the outer scope. These are variables that are captured by the closure and can be used
     * within its body.
     */
    std::vector<std::unique_ptr<ClosureExpressionCaptureAst>> captures;

    /**
     * Construct the ClosureExpressionCaptureGroupAst with the arguments matching the members.
     * @param[in] tok_caps The @c caps token that indicates the start of the closure capture group.
     * @param[in] captures The captured variables from the outer scope.
     */
    explicit ClosureExpressionCaptureGroupAst(
        decltype(tok_caps) &&tok_caps,
        decltype(captures) &&captures);

    ~ClosureExpressionCaptureGroupAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
