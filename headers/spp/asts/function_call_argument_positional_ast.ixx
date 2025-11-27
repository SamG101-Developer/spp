module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_positional_ast;
import spp.asts.function_call_argument_ast;

import std;


/**
 * The FunctionCallArgumentPositionalAst represents a positional argument in a function call. It is forces the argument
 * to be matched by an index rather than a keyword. It also support for unpacking a tuple into arguments.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentPositionalAst : FunctionCallArgumentAst {
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
        decltype(conv) &&conv,
        decltype(tok_unpack) &&tok_unpack,
        decltype(val) &&val);

    ~FunctionCallArgumentPositionalAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
