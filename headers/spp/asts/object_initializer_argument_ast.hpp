#ifndef OBJECT_INITIALIZER_ARGUMENT_AST_HPP
#define OBJECT_INITIALIZER_ARGUMENT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ObjectInitializerArgumentAst is the base class representing an argument in a object initialization. It is
 * inherited into the "shorthand" and "keyword" variants.
 */
struct spp::asts::ObjectInitializerArgumentAst : Ast {
    using Ast::Ast;

    /**
     * The expression that is being passed as the argument to the object initialization. Both positional and keyword
     * arguments have a value.
     */
    std::unique_ptr<ExpressionAst> val;

    /**
     * Construct the ObjectInitializerArgumentAst with the arguments matching the members.
     * @param val The expression that is being passed as the argument to the object initialization.
     */
    explicit ObjectInitializerArgumentAst(
        decltype(val) &&val);
};


#endif //OBJECT_INITIALIZER_ARGUMENT_AST_HPP
