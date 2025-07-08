#ifndef ITER_EXPRESSION_AST_HPP
#define ITER_EXPRESSION_AST_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IterExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c iter token that indicates the start of an iteration expression.
     */
    std::unique_ptr<TokenAst> tok_iter;

    /**
     * The generated value being inspected. This will be from resuming a coroutine.
     */
    std::unique_ptr<ExpressionAst> cond;

    /**
     * The @c of keyword that indicated pattern matching. This mirrors the @c case-of syntax, but is used for a
     * coroutine generated value.
     */
    std::unique_ptr<TokenAst> tok_of;

    /**
     * The body of the iteration expression. This is a inner scope of @c iter block branches that each contain a
     * pattern, like @c case branches do.
     */
    std::vector<std::unique_ptr<IterExpressionBranchAst>> branches;

    /**
     * Construct the IterExpressionAst with the arguments matching the members.
     * @param[in] tok_iter The @c iter token that indicates the start of an iteration expression.
     * @param[in] cond The generated value being inspected.
     * @param[in] tok_of The @c of keyword that indicated pattern matching.
     * @param[in] branches The body of the iteration expression.
     */
    IterExpressionAst(
        decltype(tok_iter) &&tok_iter,
        decltype(cond) &&cond,
        decltype(tok_of) &&tok_of,
        decltype(branches) &&branches);
};


#endif //ITER_EXPRESSION_AST_HPP
