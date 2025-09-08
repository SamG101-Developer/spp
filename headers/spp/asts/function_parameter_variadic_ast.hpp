#pragma once
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionParameterVariadicAst represents a variadic parameter in a function prototype. It is used to define
 * parameters that can accept an arbitrary number of arguments, such as @c *args in Python.
 */
struct spp::asts::FunctionParameterVariadicAst final : FunctionParameterAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the @c .. ellipsis in the function parameter. This indicates that the parameter is
     * variadic, meaning it can accept a variable number of arguments.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Construct the FunctionParameterVariadicAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the function parameter.
     */
    FunctionParameterVariadicAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type);

    ~FunctionParameterVariadicAst() override;
};
