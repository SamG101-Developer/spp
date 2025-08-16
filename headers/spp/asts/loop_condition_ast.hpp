#ifndef LOOP_CONDITION_AST_HPP
#define LOOP_CONDITION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


struct spp::asts::LoopConditionAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;
};


#endif //LOOP_CONDITION_AST_HPP
