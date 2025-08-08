#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/expression_ast.hpp>


spp::asts::ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
    decltype(val) &&val):
    val(std::move(val)) {
}
