#pragma once
#include <spp/asts/iter_pattern_variant_ast.hpp>


struct spp::asts::IterPatternVariantNoValueAst final : IterPatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c _ token that indicates the pattern variant does not have a value. This is used with @c GenOpt generators.
     */
    std::unique_ptr<TokenAst> tok_underscore;

    /**
     * Constructor for the @c IterPatternVariantNoValueAst.
     * @param tok_underscore The @c ? token that indicates the pattern variant does not have a value.
     */
    explicit IterPatternVariantNoValueAst(
        decltype(tok_underscore) &&tok_underscore);

    ~IterPatternVariantNoValueAst() override;
};
