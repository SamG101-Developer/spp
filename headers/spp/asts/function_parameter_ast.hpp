#ifndef FUNCTION_PARAMETER_AST_HPP
#define FUNCTION_PARAMETER_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionParameterAst provides a common base to all parameter types in a function prototype. It is inherited by
 * the required, optional, variadic and self parameters, and provides the common functionality for all of them.
 */
struct spp::asts::FunctionParameterAst : Ast {
    /**
     * The local variable declaration for this parameter. This is used to create a local variable for the parameter,
     * using the same syntax as variables, such as destructuring.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * The token that represents the @c : colon in the function parameter. This separates the parameter name from the
     * type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the parameter. This is used to specify the type of the parameter, such as @c I32 or @c F64 . This is
     * a required field, as the type of the parameter must be known at compile time.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * Construct the FunctionParameterAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     */
    FunctionParameterAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);
};


#endif //FUNCTION_PARAMETER_AST_HPP
