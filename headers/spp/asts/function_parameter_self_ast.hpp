#ifndef FUNCTION_PARAMETER_SELF_HPP
#define FUNCTION_PARAMETER_SELF_HPP

#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The convention is attached to the self parameter rather than its type, as it is required before the type is
     * necessarily attached.
     */
    std::unique_ptr<ConventionAst> tok_conv;

    /**
     * Construct the FunctionParameterSelfAst with the arguments matching the members.
     * @param tok_conv The convention token for this parameter.
     * @param var The local variable declaration for this parameter.
     */
    FunctionParameterSelfAst(
        decltype(tok_conv) &&tok_conv,
        decltype(var) &&var);
};


#endif //FUNCTION_PARAMETER_SELF_HPP
