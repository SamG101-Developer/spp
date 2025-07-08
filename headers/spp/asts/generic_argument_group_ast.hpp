#ifndef GENERIC_ARGUMENT_GROUP_AST_HPP
#define GENERIC_ARGUMENT_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericArgumentGroupAst : Ast {
    /**
     * The token that represents the left bracket @code [@endcode in the generic argument group. This introduces the
     * generic argument group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of arguments in the generic argument group. This can contain both positional and keyword arguments.
     */
    std::vector<std::unique_ptr<GenericArgumentAst>> args;

    /**
     * The token that represents the right parenthesis @code ]@endcode in the generic call argument group. This closes
     * the generic argument group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the GenericArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic argument group.
     * @param args The list of arguments in the generic argument group.
     * @param tok_r The token that represents the right parenthesis @code ]@endcode in the generic call argument group.
     */
    GenericArgumentGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(args) &&args,
        decltype(tok_r) &&tok_r);
};


#endif //GENERIC_ARGUMENT_GROUP_AST_HPP
