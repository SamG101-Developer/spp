#ifndef OBJECT_INITIALIZER_ARGUMENT_GROUP_AST_HPP
#define OBJECT_INITIALIZER_ARGUMENT_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ObjectInitializerArgumentGroupAst represents a group of object initializer arguments. It is used to group
 * multiple shorthand or keyword arguments together in a object initializer.
 */
struct spp::asts::ObjectInitializerArgumentGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left parenthesis @code (@endcode in the object initializer argument group. This
     * introduces the object initializer argument group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of arguments in the object initializer argument group. This can contain both positional and keyword
     * arguments.
     */
    std::vector<std::unique_ptr<ObjectInitializerArgumentAst>> args;

    /**
     * The token that represents the right parenthesis @code )@endcode in the object initializer argument group. This
     * closes the object initializer argument group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ObjectInitializerArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the object initializer argument
     * group.
     * @param args The list of arguments in the object initializer argument group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the object initializer argument
     * group.
     */
    ObjectInitializerArgumentGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(args) &&args,
        decltype(tok_r) &&tok_r);
};


#endif //OBJECT_INITIALIZER_ARGUMENT_GROUP_AST_HPP
