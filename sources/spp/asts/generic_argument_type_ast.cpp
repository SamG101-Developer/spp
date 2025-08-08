#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericArgumentTypeAst::GenericArgumentTypeAst(
    decltype(val) &&val):
    val(std::move(val)) {
}
