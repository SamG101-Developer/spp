#ifndef GENERIC_PARAMETER_GROUP_AST_HPP
#define GENERIC_PARAMETER_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterGroupAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left bracket @code [@endcode in the generic parameter group. This introduces the
     * generic parameter group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of parameters in the generic parameter group. This can contain both required and optional parameters.
     */
    std::vector<std::unique_ptr<GenericParameterAst>> params;

    /**
     * The token that represents the right bracket @code ]@endcode in the generic parameter group. This closes the
     * generic parameter group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the GenericParameterGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic parameter group.
     * @param params The list of parameters in the generic parameter group.
     * @param tok_r The token that represents the right bracket @code ]@endcode in the generic parameter group.
     */
    GenericParameterGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(params) &&params,
        decltype(tok_r) &&tok_r);
};


#endif //GENERIC_PARAMETER_GROUP_AST_HPP
