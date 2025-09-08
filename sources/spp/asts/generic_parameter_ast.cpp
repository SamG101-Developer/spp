#include <spp/asts/generic_parameter_ast.hpp>


spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name) :
    name(std::move(name)) {
}
