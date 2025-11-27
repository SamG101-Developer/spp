module;
#include <spp/macros.hpp>

export module spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.iter_pattern_variant_ast;

import std;


SPP_EXP_CLS struct spp::asts::IterPatternVariantExhaustedAst final : IterPatternVariantAst {
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

    SPP_AST_KEY_FUNCTIONS;
};
