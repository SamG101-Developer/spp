#ifndef ITER_EXPRESSION_BRANCH_AST_HPP
#define ITER_EXPRESSION_BRANCH_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IterExpressionBranchAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The pattern that this branch matches against. This can only be a single pattern due to different @c iter patterns
     * introducing different variables and symbols (same as case-of destructuring).
     */
    std::unique_ptr<IterPatternVariantAst> patterns;

    /**
     * The body of the iteration branch. This is an inner scope that contains the statements that will be executed if
     * the branch matches.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * An optional guard for the iteration branch. This is a boolean expression that must evaluate to true for the
     * branch to be executed.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * Construct the IterExpressionBranchAst with the arguments matching the members.
     * @param patterns The pattern that this branch matches against.
     * @param body The body of the iteration branch.
     * @param guard The optional guard for the iteration branch.
     */
    IterExpressionBranchAst(
        decltype(patterns) &&patterns,
        decltype(body) &&body,
        decltype(guard) &&guard);

    ~IterExpressionBranchAst() override;
};


#endif //ITER_EXPRESSION_BRANCH_AST_HPP
