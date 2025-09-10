#pragma once
#include <spp/asts/loop_condition_ast.hpp>


struct spp::asts::LoopConditionIterableAst final : LoopConditionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The variable for iteration. This is filled with each element of the iterable as the loop iterates.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * The token that represents the @c in keyword in the loop condition. This is used to indicate that the loop is
     * iterating over an iterable, and separates the variable from the iterable.
     */
    std::unique_ptr<TokenAst> tok_in;

    /**
     * The iterable expression that the loop will iterate over. This can be any expression that evaluates to a generator
     * type, typically the @code .iter_xxx@endcode family of methods.
     */
    std::unique_ptr<ExpressionAst> iterable;

    /**
     * Construct the LoopConditionIterableAst with the arguments matching the members.
     * @param var The variable for iteration.
     * @param tok_in The token that represents the @c in keyword in the loop condition.
     * @param iterable The iterable expression that the loop will iterate over.
     */
    LoopConditionIterableAst(
        decltype(var) &&var,
        decltype(tok_in) &&tok_in,
        decltype(iterable) &&iterable);

    ~LoopConditionIterableAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
