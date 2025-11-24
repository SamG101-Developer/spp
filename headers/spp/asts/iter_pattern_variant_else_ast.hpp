#pragma once

#include <spp/asts/iter_pattern_variant_ast.hpp>


struct spp::asts::IterPatternVariantElseAst final : IterPatternVariantAst {
    /**
     * The @c else token that indicates the pattern variant is a "catch-all". This is used with any generator.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * Constructor for the @c IterPatternVariantElseAst.
     * @param tok_else The @c else token that indicates the pattern variant is a "catch-all".
     */
    explicit IterPatternVariantElseAst(
        decltype(tok_else) &&tok_else);

    ~IterPatternVariantElseAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
