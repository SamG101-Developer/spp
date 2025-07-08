#ifndef OBJECT_INITIALIZER_ARGUMENT_SHORTHAND_AST_HPP
#define OBJECT_INITIALIZER_ARGUMENT_SHORTHAND_AST_HPP

#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ObjectInitializerArgumentShorthandAst represents a shorthand argument in an object initializer. It is forces the
 * argument to be matched by shorthand value rather than a keyword.
 */
struct spp::asts::ObjectInitializerArgumentShorthandAst final : ObjectInitializerArgumentAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional @c .. token that indicates an "else" argument. This fills all the missing attributes in the object
     * with the corresponding attributes from this argument.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Construct the ObjectInitializerArgumentShorthandAst with the arguments matching the members.
     * @param tok_ellipsis The optional @c .. token that indicates an "else" argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     *
     * @note This constructor just calls the ObjectInitializerArgumentAst constructor with the same arguments, but is
     * defined for uniformity with the other argument variants.
     */
    explicit ObjectInitializerArgumentShorthandAst(
        std::unique_ptr<TokenAst> tok_ellipsis,
        std::unique_ptr<ExpressionAst> &&val);
};


#endif //OBJECT_INITIALIZER_ARGUMENT_SHORTHAND_AST_HPP
