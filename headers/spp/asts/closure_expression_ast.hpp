#ifndef CLOSURE_EXPRESSION_AST_HPP
#define CLOSURE_EXPRESSION_AST_HPP


#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ClosureExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional @c cor keyword. Providing this will turn the closure into a coroutine closure.
     */
    std::unique_ptr<TokenAst> tok_cor;

    /**
     * The parameter and capture group of the closure. This will contain the parameters for the closure, as well as any
     * captured variables from the outer scopes.
     */
    std::unique_ptr<ClosureExpressionParameterAndCaptureGroupAst> pc_group;

    /**
     * The body of the closure. This can be a single expression, like @code || 1 + 2 ||@endcode, or an inner scope
     * (type of expression), for more complex closures.
     */
    std::unique_ptr<ExpressionAst> body;

    /**
     * Construct the ClosureExpressionAst with the arguments matching the members.
     * @param tok_cor The optional @c cor keyword.
     * @param pc_group The parameter and capture group of the closure.
     * @param body The body of the closure.
     */
    ClosureExpressionAst(
        decltype(tok_cor) &&tok_cor,
        decltype(pc_group) &&pc_group,
        decltype(body) &&body);

    ~ClosureExpressionAst() override;

private:
    std::unique_ptr<TypeAst> m_return_type;
};


#endif //CLOSURE_EXPRESSION_AST_HPP
