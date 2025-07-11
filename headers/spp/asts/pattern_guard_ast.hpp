#ifndef PATTERN_GUARD_AST_HPP
#define PATTERN_GUARD_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::PatternGuardAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c and keyword token. This is used to indicate that the pattern guard is being introduced, following a
     * pattern.
     */
    std::unique_ptr<TokenAst> tok_and;

    /**
     * The expression that is used as the guard for the pattern. This expression is evaluated to determine if the
     * pattern matches, and must be a boolean expression.
     */
    std::unique_ptr<ExpressionAst> expression;

    /**
     * Constructor for the @c PatternGuardAst.
     * @param tok_and The @c and keyword token.
     * @param expression The expression that is used as the guard for the pattern.
     */
    PatternGuardAst(
        decltype(tok_and) &&tok_and,
        decltype(expression) &&expression);
};


#endif //PATTERN_GUARD_AST_HPP
