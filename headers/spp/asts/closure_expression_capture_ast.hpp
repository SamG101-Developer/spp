#ifndef CLOSURE_EXPRESSION_CAPTURE__AST_HPP
#define CLOSURE_EXPRESSION_CAPTURE__AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ClosureExpressionCaptureAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The convention applied to the captured variable. This is used to indicate how the variable should be captured,
     * for example immutably borrowed into the closure, etc.
     */
    std::unique_ptr<ConventionAst> conv;

    /**
     * The name of the symbol that is being captured by the closure. This is the identifier that is used to refer to the
     * symbol in the scope that the closure is defined in.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * Construct the ClosureExpressionCaptureAst with the arguments matching the members.
     * @param conv The convention applied to the captured variable.
     * @param name The name of the symbol that is being captured by the closure.
     */
    ClosureExpressionCaptureAst(
        decltype(conv) &&conv,
        decltype(name) &&name);
};


#endif //CLOSURE_EXPRESSION_CAPTURE__AST_HPP
