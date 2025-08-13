#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::SupPrototypeExtensionAst::SupPrototypeExtensionAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(tok_ext) &&tok_ext,
    decltype(super_class) &&super_class,
    decltype(impl) &&impl):
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    tok_ext(std::move(tok_ext)),
    super_class(std::move(super_class)),
    impl(std::move(impl)) {
}


auto spp::asts::SupPrototypeExtensionAst::pos_start() const -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeExtensionAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


spp::asts::SupPrototypeExtensionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_ext);
    SPP_STRING_APPEND(super_class);
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeExtensionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sup);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_ext);
    SPP_PRINT_APPEND(super_class);
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}
