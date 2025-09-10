#pragma once
#include <spp/asts/ast.hpp>


/**
 * The ClassMemberAst class is the base class for all members of a class in the abstract syntax tree. This is only
 * applicable to the ClassAttributeAst, but allows for simple expansion in the future,
 */
struct spp::asts::ClassMemberAst: virtual Ast {
};
