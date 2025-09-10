#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * Construct the CasePatternVariantElseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    explicit CasePatternVariantElseAst(
        decltype(tok_else) &&tok_else);

    ~CasePatternVariantElseAst() override;
};
