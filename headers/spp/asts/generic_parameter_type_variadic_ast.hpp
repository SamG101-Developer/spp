#ifndef GENERIC_PARAMETER_TYPE_VARIADIC_AST_HPP
#define GENERIC_PARAMETER_TYPE_VARIADIC_AST_HPP

#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterTypeVariadicAst final : GenericParameterTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the @c .. ellipsis in the generic parameter. This indicates that the parameter is
     * variadic, meaning it can accept multiple values.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Construct the GenericParameterTypeVariadicAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the generic parameter.
     */
    GenericParameterTypeVariadicAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        decltype(name) &&name,
        decltype(constraints) &&constraints);
};


#endif //GENERIC_PARAMETER_TYPE_VARIADIC_AST_HPP
