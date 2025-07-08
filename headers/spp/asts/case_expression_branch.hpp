#ifndef CASE_EXPRESSION_BRANCH_HPP
#define CASE_EXPRESSION_BRANCH_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * A CaseExpressionBranchAst represents a branch in a case expression. A branch contains patterns that are matched
 * against the condition of the case expression. Operators can optionally be defined for "of" pattern matching.
 */
struct spp::asts::CaseExpressionBranchAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional operator token for partial fragment pattern matching. This isused to indiciate how to match a
     * pattern, such as equality, comparison, destructuring, etc.
     */
    std::unique_ptr<TokenAst> op;

    /**
     * The list of patterns that this branch matches against. More than 1 pattern is valid for non-destructuring
     * operations.
     */
    std::vector<std::unique_ptr<PatternVariantAst>> patterns;

    /**
     * For destructuring operations, a guard can be added to refine when destructuring should take place.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * The body of the case expression branch. This is where the statements that are executed if the branch matches
     * one of the patterns.
     */
    std::unique_ptr<InnerScopeExpressionAst<StatementAst>> body;

    /**
     * Construct the CaseExpressionBranchAst with the arguments matching the members.
     * @param[in] pos The position of the AST in the source code.
     * @param[in] op The optional operator token for partial fragment pattern matching.
     * @param[in] patterns The list of patterns that this branch matches against.
     * @param[in] guard The optional guard for destructuring operations.
     * @param[in] body The body of the case expression branch.
     */
    CaseExpressionBranchAst(
        decltype(op) &&op,
        decltype(patterns) &&patterns,
        decltype(guard) &&guard,
        decltype(body) &&body);
};


#endif //CASE_EXPRESSION_BRANCH_HPP
