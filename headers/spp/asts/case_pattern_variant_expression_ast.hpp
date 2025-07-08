#ifndef CASE_PATTERN_VARIANT_EXPRESSION_AST_HPP
#define CASE_PATTERN_VARIANT_EXPRESSION_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The expression that is used in the case pattern variant. This is the expression that will be matched against the
     * condition from the @c case statement.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the CasePatternVariantExpressionAst with the arguments matching the members.
     * @param expr The expression that is used in the case pattern variant.
     */
    explicit CasePatternVariantExpressionAst(
        decltype(expr) &&expr);
};


#endif //CASE_PATTERN_VARIANT_EXPRESSION_AST_HPP
