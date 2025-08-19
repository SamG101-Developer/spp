#ifndef COROUTINE_PROTOTYPE_AST_HPP
#define COROUTINE_PROTOTYPE_AST_HPP

#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CoroutinePrototypeAst : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //COROUTINE_PROTOTYPE_AST_HPP
