module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_ast;

import std;


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst final : CasePatternVariantAst {
    /**
     * The @c .. token indicates the skip multiple arguments pattern. This is used to indicate that a group of arguments
     * is being skipped. Bindings are used for array and tuple destructuring, while object destructuring can only use an
     * unbound multi skip.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * The optional binding for the skip multiple arguments pattern. This is used to bind the skipped arguments to a
     * variable, as an inner array or tuple (based on the outer type being destructured). No biding means that these
     * values are dropped.
     */
    std::unique_ptr<CasePatternVariantSingleIdentifierAst> binding;

    /**
     * Construct the CasePatternVariantDestructureSkipMultipleArgumentsAst with the arguments matching the members.
     * @param tok_ellipsis The @c .. token that indicates the skip multiple arguments pattern.
     * @param binding The optional binding for the skip multiple arguments pattern.
     */
    CasePatternVariantDestructureSkipMultipleArgumentsAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        std::unique_ptr<CasePatternVariantAst> &&binding);

    ~CasePatternVariantDestructureSkipMultipleArgumentsAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;
};
