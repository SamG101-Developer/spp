#pragma once
#include <spp/asts/function_parameter_ast.hpp>


struct spp::asts::FunctionParameterRequiredAst final : FunctionParameterAst {
    /**
     * Construct the FunctionParameterRequiredAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     *
     * @note This constructor just calls the FunctionParameterAst constructor with the same arguments, but is defined
     * for uniformity with the other parameter variants.
     */
    FunctionParameterRequiredAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type);

    ~FunctionParameterRequiredAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
