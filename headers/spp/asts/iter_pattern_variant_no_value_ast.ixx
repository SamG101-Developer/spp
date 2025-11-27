module;
#include <spp/macros.hpp>

export module spp.asts.iter_pattern_variant_no_value_ast;
import spp.asts.iter_pattern_variant_ast;

import std;


SPP_EXP_CLS struct spp::asts::IterPatternVariantNoValueAst final : IterPatternVariantAst {
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

    SPP_AST_KEY_FUNCTIONS;
};
