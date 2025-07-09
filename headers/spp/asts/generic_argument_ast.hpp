#ifndef GENERIC_ARGUMENT_AST_HPP
#define GENERIC_ARGUMENT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the GenericArgumentCompAst and
 * GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
struct spp::asts::GenericArgumentAst : virtual Ast {
    using Ast::Ast;
};


#endif //GENERIC_ARGUMENT_AST_HPP
