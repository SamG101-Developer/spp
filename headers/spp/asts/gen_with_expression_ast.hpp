#ifndef GEN_WITH_EXPRESSION_AST_HPP
#define GEN_WITH_EXPRESSION_AST_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
     * the coroutine is suspending its execution and yielding a value.
     */
    std::unique_ptr<TokenAst> tok_gen;

    /**
     * The token that represents the @c with keyword in the source code. This indicates that the expression is being
     * generated with a specific context.
     */
    std::unique_ptr<TokenAst> tok_with;

    /**
     * The expression that is being generated with the context. This is the value that will be used in the generation.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the GenWithExpressionAst with the arguments matching the members.
     * @param tok_gen The token that represents the @c gen keyword in the source code.
     * @param tok_with The token that represents the @c with keyword in the source code.
     * @param expr The expression that is being generated with the context.
     */
    GenWithExpressionAst(
        decltype(tok_gen) &&tok_gen,
        decltype(tok_with) &&tok_with,
        decltype(expr) &&expr);

    ~GenWithExpressionAst() override;
};


#endif //GEN_WITH_EXPRESSION_AST_HPP
