#pragma once
#include <spp/asts/iter_pattern_variant_ast.hpp>


struct spp::asts::IterPatternVariantVariableAst final : IterPatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The variable that the generator's yielded value is bound to. This allows the internal value inside the
     * @c Generated container to be inspected.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * Constructor for the @c IterPatternVariantVariableAst.
     * @param var The variable that the generator's yielded value is bound to.
     */
    explicit IterPatternVariantVariableAst(
        decltype(var) &&var);

    ~IterPatternVariantVariableAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
