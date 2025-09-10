#pragma once
#include <spp/asts/iter_pattern_variant_ast.hpp>


struct spp::asts::IterPatternVariantExhaustedAst final : IterPatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c !! token that indicates the pattern variant is exhausted. This is used with any @c GenXXX generators.
     */
    std::unique_ptr<TokenAst> tok_exhausted;

    /**
     * Constructor for the @c IterPatternVariantExhaustedAst.
     * @param tok_exhausted The @c exhausted token that indicates the pattern variant is exhausted.
     */
    explicit IterPatternVariantExhaustedAst(
        decltype(tok_exhausted) &&tok_exhausted);

    ~IterPatternVariantExhaustedAst() override;
};
