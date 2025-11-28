module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts._fwd;
import spp.asts.generic_parameter_comp_ast;

import std;


SPP_EXP_CLS struct spp::asts::GenericParameterCompVariadicAst final : GenericParameterCompAst {
    /**
     * The token that represents the @c .. ellipsis in the generic parameter. This indicates that the parameter is
     * variadic, meaning it can accept multiple values.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Construct the GenericParameterCompVariadicAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the generic parameter.
     */
    GenericParameterCompVariadicAst(
        decltype(tok_cmp) &&tok_cmp,
        decltype(tok_ellipsis) &&tok_ellipsis,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);

    ~GenericParameterCompVariadicAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
