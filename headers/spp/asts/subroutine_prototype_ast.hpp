#ifndef SUBROUTINE_PROTOTYPE_AST_HPP
#define SUBROUTINE_PROTOTYPE_AST_HPP

#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::SubroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;
};


#endif //SUBROUTINE_PROTOTYPE_AST_HPP
