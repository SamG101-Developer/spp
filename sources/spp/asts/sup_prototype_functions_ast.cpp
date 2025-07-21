#include <spp/asts/sup_prototype_functions_ast.hpp>


spp::asts::SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(body) &&body):
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    body(std::move(body)) {
}
