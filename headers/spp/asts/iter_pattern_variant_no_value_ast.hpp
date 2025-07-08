#ifndef ITER_PATTERN_VARIANT_NO_VALUE_AST_HPP
#define ITER_PATTERN_VARIANT_NO_VALUE_AST_HPP

#include <spp/asts/iter_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IterPatternVariantNoValueAst final : IterPatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c ? token that indicates the pattern variant does not have a value. This is used with @c GenOpt generators.
     */
    std::unique_ptr<TokenAst> tok_qst;

    /**
     * Constructor for the @c IterPatternVariantNoValueAst.
     * @param tok_qst The @c ? token that indicates the pattern variant does not have a value.
     */
    explicit IterPatternVariantNoValueAst(
        decltype(tok_qst) &&tok_qst);
};


#endif //ITER_PATTERN_VARIANT_NO_VALUE_AST_HPP
