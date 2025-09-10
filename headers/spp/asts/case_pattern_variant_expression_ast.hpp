#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The expression that is used in the case pattern variant. This is the expression that will be matched against the
     * condition from the @c case statement.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the CasePatternVariantExpressionAst with the arguments matching the members.
     * @param expr The expression that is used in the case pattern variant.
     */
    explicit CasePatternVariantExpressionAst(
        decltype(expr) &&expr);

    ~CasePatternVariantExpressionAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
