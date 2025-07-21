#include <spp/asts/sup_prototype_extension_ast.hpp>


spp::asts::SupPrototypeExtensionAst::SupPrototypeExtensionAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(tok_ext) &&tok_ext,
    decltype(super_class) &&super_class,
    decltype(body) &&body):
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    tok_ext(std::move(tok_ext)),
    super_class(std::move(super_class)),
    body(std::move(body)) {
}
