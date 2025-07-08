#ifndef GENERIC_PARAMETER_COMP_REQUIRED_AST_HPP
#define GENERIC_PARAMETER_COMP_REQUIRED_AST_HPP

#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterCompRequiredAst final : GenericParameterCompAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * Construct the GenericParameterCompAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     *
     * @note This constructor just calls the GenericParameterCompAst constructor with the same arguments, but is defined
     * for uniformity with the other parameter variants.
     */
    GenericParameterCompRequiredAst(
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);
};


#endif //GENERIC_PARAMETER_COMP_REQUIRED_AST_HPP
