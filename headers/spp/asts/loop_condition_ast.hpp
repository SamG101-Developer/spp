#ifndef LOOP_CONDITION_AST_HPP
#define LOOP_CONDITION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LoopConditionAst : virtual Ast {
    using Ast::Ast;
};


#endif //LOOP_CONDITION_AST_HPP
