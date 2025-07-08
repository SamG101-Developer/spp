#ifndef LOCAL_VARIABLE_SINGLE_IDENTIFIER_AST_HPP
#define LOCAL_VARIABLE_SINGLE_IDENTIFIER_AST_HPP

#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The LocalVariableSingleIdentifierAst represents a local variable that is defined by a single identifier. This is used
 * to define variables in the local scope, such as in function bodies. A typical example of this @code mut x@endcode.
 * This defines a local variable called @c x that is mutable.
 *
 * @n
 * This AST can be nested inside other local variable ASTs, such as a tuple destructure,
 * ie @code (mut x, y) = (1, 2)@endcode. As this AST is either used as a parameter or part of a "let" statement, it will
 * be introducing a symbol into the local scope.
 */
struct spp::asts::LocalVariableSingleIdentifierAst final : LocalVariableAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional mutability token. If the "@c mut keyword was provided, then this will be given a value. Otherwise,
     * it will be @c nullptr. This is used to indicate that the variable is mutable, and can be modified after being
     * assigned its initial value.
     */
    std::unique_ptr<TokenAst> tok_mut;

    /**
     * The name of the local variable. This is the identifier that is used to refer to the variable in the local scope.
     * It will be saved against the symbol in the symbol table of the current scope.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The optional alias for the local variable. This is used to create an alias for the variable, which can be used to
     * refer to the variable by a different name. This is useful in destructuring, to prevent conflicting variables when
     * types have the same name attributes: @code case my_value is Some(val as alias)@endcode.
     */
    std::unique_ptr<LocalVariableSingleIdentifierAliasAst> alias;

    /**
     * Construct the LocalVariableSingleIdentifierAst with the arguments matching the members.
     * @param tok_mut The optional mutability token.
     * @param name The name of the local variable.
     * @param alias The optional alias for the local variable.
     */
    LocalVariableSingleIdentifierAst(
        decltype(tok_mut) &&tok_mut,
        decltype(name) &&name,
        decltype(alias) &&alias);
};


#endif //LOCAL_VARIABLE_SINGLE_IDENTIFIER_AST_HPP
