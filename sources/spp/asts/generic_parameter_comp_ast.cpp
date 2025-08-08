#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterCompAst::GenericParameterCompAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type):
    tok_cmp(std::move(tok_cmp)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}
