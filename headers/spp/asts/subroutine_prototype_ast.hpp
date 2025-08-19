#ifndef SUBROUTINE_PROTOTYPE_AST_HPP
#define SUBROUTINE_PROTOTYPE_AST_HPP

#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::SubroutinePrototypeAst : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //SUBROUTINE_PROTOTYPE_AST_HPP
