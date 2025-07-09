#ifndef CASE_EXPRESSION_BRANCH_AST_HPP
#define CASE_EXPRESSION_BRANCH_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CaseExpressionBranchAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional comparison operator. This is for cases pattern matching cases that look something like
     * @code== 123 { ... }@endcode.
     */
    std::unique_ptr<TokenAst> tok_comparison;

    /**
     * The list of patterns that this branch matches against. There can be more than 1 pattern for non-destructuring
     * operations.
     */
    std::vector<std::unique_ptr<CasePatternVariantAst>> patterns;

    /**
     * The optional guard for the case branch. This is a boolean expression that must evaluate to true for destructuring
     * patterns only.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * The body of the case branch. This is an inner scope that contains the statements that will be executed if the
     * branch matches.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * Construct the CaseExpressionBranchAst with the arguments matching the members.
     * @param tok_comparison The optional comparison operator.
     * @param patterns The list of patterns that this branch matches against.
     * @param guard The optional guard for the case branch.
     * @param body The body of the case branch.
     */
    CaseExpressionBranchAst(
        decltype(tok_comparison) &&tok_comparison,
        decltype(patterns) &&patterns,
        decltype(guard) &&guard,
        decltype(body) &&body);
};


#endif //CASE_EXPRESSION_BRANCH_AST_HPP
