module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_variadic_ast;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterVariadicAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The FunctionParameterVariadicAst represents a variadic parameter in a function prototype. It is used to define
 * parameters that can accept an arbitrary number of arguments, such as @c *args in Python.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterVariadicAst final : FunctionParameterAst {
    /**
     * The token that represents the @c .. ellipsis in the function parameter. This indicates that the parameter is
     * variadic, meaning it can accept a variable number of arguments.
     */
    Unique<TokenAst> TokEllipsis;

    /**
     * Construct the FunctionParameterVariadicAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the function parameter.
     */
    FunctionParameterVariadicAst(
        decltype(TokEllipsis) &&tok_ellipsis,
        decltype(Var) &&var,
        decltype(TokColon) &&tok_colon,
        decltype(Type) type);

    ~FunctionParameterVariadicAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
