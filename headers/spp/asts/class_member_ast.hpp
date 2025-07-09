#ifndef CLASS_MEMBER_AST_HPP
#define CLASS_MEMBER_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ClassMemberAst class is the base class for all members of a class in the abstract syntax tree. This is only
 * applicable to the ClassAttributeAst, but allows for simple expansion in the future,
 */
struct spp::asts::ClassMemberAst : virtual Ast {
    using Ast::Ast;
};


#endif //CLASS_MEMBER_AST_HPP
