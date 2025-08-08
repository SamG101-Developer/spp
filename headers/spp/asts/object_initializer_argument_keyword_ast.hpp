#ifndef OBJECT_INITIALIZER_ARGUMENT_KEYWORD_AST_HPP
#define OBJECT_INITIALIZER_ARGUMENT_KEYWORD_AST_HPP

#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ObjectInitializerArgumentKeywordAst represents a keyword argument in a object initializer. It is forces the
 * argument to be matched by a keyword rather than shorthand value.
 */
struct spp::asts::ObjectInitializerArgumentKeywordAst final : ObjectInitializerArgumentAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the keyword argument. This is the identifier that is used to refer to the argument in the function
     * call.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The token that represents the assignment operator \c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the ObjectInitializerArgumentKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator \c = in the keyword argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     */
    ObjectInitializerArgumentKeywordAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);

    ~ObjectInitializerArgumentKeywordAst() override;
};


#endif //OBJECT_INITIALIZER_ARGUMENT_KEYWORD_AST_HPP
