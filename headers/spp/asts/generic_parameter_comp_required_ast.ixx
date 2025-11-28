module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_required_ast;
import spp.asts._fwd;
import spp.asts.generic_parameter_comp_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompRequiredAst;
}


/**
 * The GenericParameterCompRequiredAst represents required generic @c cmp parameters in classes, function,
 * superimpositions etc. They look like: @code cls MyClass[cmp n: USize] { ... }@endcode.
 */
SPP_EXP_CLS struct spp::asts::GenericParameterCompRequiredAst final : GenericParameterCompAst {
    /**
     * Construct the GenericParameterCompAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @code :@endcode colon in the generic parameter.
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

    ~GenericParameterCompRequiredAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
