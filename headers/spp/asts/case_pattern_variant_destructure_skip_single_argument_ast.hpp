#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


struct spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst final : CasePatternVariantAst {
    /**
     * The @c _ token that indicates the skip single argument pattern. This is used to indicate the next element
     * sequentially is being skipped, and is often seen in array and tuple destructuring. Invalid in object
     * destructuring as it is purely keyword based, and not positional.
     */
    std::unique_ptr<TokenAst> tok_underscore;

    /**
     * Construct the CasePatternVariantDestructureSkipSingleArgumentAst with the arguments matching the members.
     * @param tok_underscore The @c _ token that indicates the skip single argument pattern.
     */
    explicit CasePatternVariantDestructureSkipSingleArgumentAst(
        decltype(tok_underscore) &&tok_underscore);

    ~CasePatternVariantDestructureSkipSingleArgumentAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(mixins::CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;
};
