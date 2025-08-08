#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterTypeAst::GenericParameterTypeAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints):
    name(std::move(name)),
    constraints(std::move(constraints)) {
}
