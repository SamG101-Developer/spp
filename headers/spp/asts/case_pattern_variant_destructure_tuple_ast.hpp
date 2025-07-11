#ifndef CASE_PATTERN_VARIANT_DESTRUCTURE_TUPLE_AST_HPP
#define CASE_PATTERN_VARIANT_DESTRUCTURE_TUPLE_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantDestructureTupleAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the tuple destructuring pattern. This is a list of patterns that will be destructured from the
     * tuple. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the CasePatternVariantDestructureTupleAst with the arguments matching the members.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     * @param[in] elems The elements of the tuple destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a tuple destructuring pattern.
     */
    CasePatternVariantDestructureTupleAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);
};


#endif //CASE_PATTERN_VARIANT_DESTRUCTURE_TUPLE_AST_HPP
