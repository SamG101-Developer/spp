#ifndef LOOP_CONDITION_BOOLEAN_HPP
#define LOOP_CONDITION_BOOLEAN_HPP

#include <spp/asts/loop_condition_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LoopConditionBooleanAst final : LoopConditionAst {
    SPP_AST_KEY_FUNCTIONS;

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

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //LOOP_CONDITION_BOOLEAN_HPP
