#ifndef FUNCTION_CALL_ARGUMENT_POSITIONAL_AST_HPP
#define FUNCTION_CALL_ARGUMENT_POSITIONAL_AST_HPP

#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionCallArgumentPositionalAst represents a positional argument in a function call. It is forces the argument
 * to be matched by an index rather than a keyword. It also support for unpacking a tuple into arguments.
 */
struct spp::asts::FunctionCallArgumentPositionalAst final : FunctionCallArgumentAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the @c .. unpacking operator. This is used to indicate that the argument is a tuple
     * being unpacked into the resulting arguments.
     */
    std::unique_ptr<TokenAst> tok_unpack;

    /**
     * Construct the FunctionCallArgumentPositionalAst with the arguments matching the members.
     * @param conv The convention on the argument being passed into the function call.
     * @param val The expression that is being passed as the argument to the function call.
     * @param tok_unpack The token that represents the @c .. unpacking operator.
     */
    FunctionCallArgumentPositionalAst(
        std::unique_ptr<TokenAst> &&tok_unpack,
        std::unique_ptr<ConventionAst> &&conv,
        std::unique_ptr<ExpressionAst> &&val);
};


#endif //FUNCTION_CALL_ARGUMENT_POSITIONAL_AST_HPP
