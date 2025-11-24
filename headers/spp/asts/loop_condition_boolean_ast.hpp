#pragma once
#include <spp/asts/loop_condition_ast.hpp>


struct spp::asts::LoopConditionBooleanAst final : LoopConditionAst {
    /**
     * The expression that represents the condition of the loop. This is a boolean expression that evaluates to a
     * boolean value.
     */
    std::unique_ptr<ExpressionAst> cond;

    /**
     * Construct the LoopConditionBoolean with the arguments matching the members.
     * @param[in] cond The expression that represents the condition of the loop.
     */
    explicit LoopConditionBooleanAst(
        decltype(cond) &&cond);

    ~LoopConditionBooleanAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
