#ifndef CASE_PATTERN_VARIANT_ELSE_CASE_AST_HPP
#define CASE_PATTERN_VARIANT_ELSE_CASE_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * The case expression that is used for the else branch.
     */
    std::unique_ptr<CaseExpressionAst> case_expr;

    /**
     * Construct the CasePatternVariantElseCaseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     * @param case_expr The case expression that is used for the else branch.
     */
    explicit CasePatternVariantElseCaseAst(
        decltype(tok_else) &&tok_else,
        decltype(case_expr) &&case_expr);
};


#endif //CASE_PATTERN_VARIANT_ELSE_CASE_AST_HPP
