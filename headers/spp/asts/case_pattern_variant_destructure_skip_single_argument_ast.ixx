module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureSkipSingleArgumentAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst final : CasePatternVariantAst {
    /**
     * The @c _ token that indicates the skip single argument pattern. This is used to indicate the next element
     * sequentially is being skipped, and is often seen in array and tuple destructuring. Invalid in object
     * destructuring as it is purely keyword based, and not positional.
     */
    Unique<TokenAst> TokUnderscore;

    /**
     * Construct the CasePatternVariantDestructureSkipSingleArgumentAst with the arguments matching the members.
     * @param tok_underscore The @c _ token that indicates the skip single argument pattern.
     */
    explicit CasePatternVariantDestructureSkipSingleArgumentAst(
        decltype(TokUnderscore) &&tok_underscore);

    ~CasePatternVariantDestructureSkipSingleArgumentAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
