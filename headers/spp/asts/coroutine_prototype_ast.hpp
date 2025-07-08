#ifndef COROUTINE_PROTOTYPE_AST_HPP
#define COROUTINE_PROTOTYPE_AST_HPP

#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;
};


#endif //COROUTINE_PROTOTYPE_AST_HPP
