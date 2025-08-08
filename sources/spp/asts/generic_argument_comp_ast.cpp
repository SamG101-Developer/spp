#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/expression_ast.hpp>


spp::asts::GenericArgumentCompAst::GenericArgumentCompAst(
    decltype(val) &&val):
    val(std::move(val)) {
}
