#ifndef LOOP_ELSE_STATEMENT_AST_HPP
#define LOOP_ELSE_STATEMENT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LoopElseStatementAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c else keyword that indicates this is an else statement for the loop.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * The body of the else statement. This is a block of statements that will be executed if the loop condition is
     * immediately false, or the iterable is already exhausted (no loops take place).
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * Construct the LoopElseStatementAst with the arguments matching the members.
     * @param[in] tok_else The @c else keyword that indicates this is an else statement for the loop.
     * @param[in] body The body of the else statement.
     */
    LoopElseStatementAst(
        decltype(tok_else) &&tok_else,
        decltype(body) &&body);
};


#endif //LOOP_ELSE_STATEMENT_AST_HPP
