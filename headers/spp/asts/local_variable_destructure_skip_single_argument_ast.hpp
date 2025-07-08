#ifndef LOCAL_VARIABLE_DESTRUCTURE_SKIP_SINGLE_ARGUMENT_AST_HPP
#define LOCAL_VARIABLE_DESTRUCTURE_SKIP_SINGLE_ARGUMENT_AST_HPP

#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableDestructureSkipSingleArgumentAst final : LocalVariableAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c _ token that indicates the skip single argument pattern. This is used to indicate the next element
     * sequentially is being skipped, and is often seen in array and tuple destructuring. Invalid in object
     * destructuring as it is purely keyword based, and not positional.
     */
    std::unique_ptr<TokenAst> tok_underscore;

    /**
     * Construct the LocalVariableDestructureSkipSingleArgumentAst with the arguments matching the members.
     * @param tok_underscore The @c _ token that indicates the skip single argument pattern.
     */
    explicit LocalVariableDestructureSkipSingleArgumentAst(
        decltype(tok_underscore) &&tok_underscore);
};


#endif //LOCAL_VARIABLE_DESTRUCTURE_SKIP_SINGLE_ARGUMENT_AST_HPP
